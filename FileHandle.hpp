#pragma once
#include <string>
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h> // for `open()`
#include <unistd.h> // for `read()`
#include <sys/stat.h>
#endif
#include "utf.hpp"
#include "UnixNanotime.hpp"
#include "UniqueHandle.hpp"

#ifdef __GNUC__
#define NOINLINE __attribute__((noinline))
#elif _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

class FileOpenError {};
class WrongFileNameStr {};
class FileIsAlreadyOpened {};
class AttemptToReadAClosedFile {};
class IOError {};
class AttemptToGetTimeOfAClosedFile {};
class GetFileTimeFailed {};
class GetCreationTimeIsNotImplemented {};
class FStatFailed {};
class AttemptToGetFileSizeOfAClosedFile {};

namespace detail
{
template <bool for_reading> class FileHandle
{
public:
    // Redirecting common constructors
    FileHandle(const std::string    &s) : FileHandle(s.c_str(), s.length()) {}
    FileHandle(const std::u16string &s) : FileHandle(s.c_str(), s.length()) {}

    bool  open(const std::string    &s) {return open(s.c_str(), s.length());}
    bool  open(const std::u16string &s) {return open(s.c_str(), s.length());}

#ifdef _WIN32
    UniqueHandle<HANDLE, INVALID_HANDLE_VALUE> handle;

    FileHandle() {}
    FileHandle(const char *s, size_t len) : FileHandle(utf::as_u16(utf::std::string_view(s, len))) {}
    FileHandle(const char16_t *s, size_t len) {if (!open(s, len)) throw FileOpenError();}
    FileHandle(const char16_t *s) {if (!open(s)) throw FileOpenError();}
    FileHandle(const char *s) : FileHandle(utf::as_u16(s)) {}

    bool open(const char *s, size_t len) {return open(utf::as_u16(utf::std::string_view(s, len)));}
    bool open(const char *s)             {return open(utf::as_u16(s));}
    bool open(const char16_t *s, size_t len)
    {
        if (s[len] != 0)
            throw WrongFileNameStr();
        return open(s);
    }

    bool open(const char16_t *s)
    {
        if (handle != INVALID_HANDLE_VALUE)
            throw FileIsAlreadyOpened();

        if (for_reading)
            handle = CreateFileW((wchar_t*)s, GENERIC_READ , FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        else
            handle = CreateFileW((wchar_t*)s, GENERIC_WRITE, 0              , NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        return handle != INVALID_HANDLE_VALUE;
    }

    bool is_valid() {return handle != INVALID_HANDLE_VALUE;}

    size_t read(void *buf, size_t sz)
    {
        if (handle == INVALID_HANDLE_VALUE)
            throw AttemptToReadAClosedFile();

        if (sz <= 0xFFFFFFFFu) {
            DWORD numberOfBytesRead;
            if (!ReadFile(handle, buf, (DWORD)sz, &numberOfBytesRead, NULL))
                throw IOError();
            return numberOfBytesRead;
        }
        else {
            char *b = (char*)buf;
            while (true) {
                DWORD numberOfBytesRead;
                if (!ReadFile(handle, b, (DWORD)(std::min)(sz, (size_t)0xFFFF0000), &numberOfBytesRead, NULL))
                    throw IOError();
                if (numberOfBytesRead == 0)
                    return b - (char*)buf;
                b += numberOfBytesRead;
                sz -= numberOfBytesRead;
                if (sz == 0)
                    return b - (char*)buf;
            }
        }
    }

    void close()
    {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
#else
    UniqueHandle<int, -1> fd;

    FileHandle() {}
    FileHandle(const char *s) {if (!open(s)) throw FileOpenError();}
    FileHandle(const char *s, size_t len) {if (!open(s, len)) throw FileOpenError();}
    FileHandle(const char16_t *s) : FileHandle(utf::as_str8(s)) {}
    FileHandle(const char16_t *s, size_t len) : FileHandle(utf::as_str8(utf::std::u16string_view(s, len))) {}

    bool open(const char16_t *s, size_t len) {return open(utf::as_str8(utf::std::u16string_view(s, len)));}
    bool open(const char16_t *s)             {return open(utf::as_str8(s));}
    bool open(const char *s, size_t len)
    {
        if (s[len] != 0)
            throw WrongFileNameStr();
        open(s);
    }

    bool open(const char *s)
    {
        if (fd != -1)
            throw FileIsAlreadyOpened();

        if (for_reading)
            fd = ::open(s, O_RDONLY);
        else
            fd = creat(s, 0666);
        return fd != -1;
    }

    bool is_valid() {return fd != -1;}

    size_t read(void *buf, size_t sz)
    {
        if (fd == -1)
            throw AttemptToReadAClosedFile();

        char *b = (char*)buf;
        while (true) {
            ssize_t r = ::read(fd, b, std::min(sz, (size_t)0x7ffff000)); // On Linux, read() will transfer at most 0x7ffff000 bytes
            if (r == -1)
                throw IOError();
            if (r == 0)
                return b - (char*)buf;
            b += r;
            sz -= r;
            if (sz == 0)
                return b - (char*)buf;
        }
    }

    void close()
    {
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
    }
#endif
#if !defined(_MSC_VER) || _MSC_VER > 1800 // unfortunately, MSVC 2013 doesn't support defaulted move constructors
    FileHandle(FileHandle &&) = default;
#else
    FileHandle(FileHandle &&fh) : handle(std::move(fh.handle)), creation_time(fh.creation_time), last_write_time(fh.last_write_time), file_size(fh.file_size) {}
#endif
    FileHandle &operator=(FileHandle &&fh)
    {
        move_assign(this, std::move(fh));
        return *this;
    }

    ~FileHandle() {close();}

    // File times and file size
private:
    UnixNanotime creation_time = UnixNanotime::uninitialized(),
               last_write_time = UnixNanotime::uninitialized();
    int64_t file_size = -1;
#ifdef _WIN32
    static const int64_t _1601_TO_1970 = 116444736000000000i64; // number of 100 nanosecond units from 1/1/1601 to 1/1/1970

    void get_file_times()
    {
        if (!is_valid())
            throw AttemptToGetTimeOfAClosedFile();

        uint64_t cfiletime, mfiletime;
        if (GetFileTime(handle, (FILETIME*)&cfiletime, NULL, (FILETIME*)&mfiletime) == 0)
            throw GetFileTimeFailed();

        creation_time   = UnixNanotime::from_nanotime_t(int64_t(cfiletime - _1601_TO_1970) * 100);
        last_write_time = UnixNanotime::from_nanotime_t(int64_t(mfiletime - _1601_TO_1970) * 100);
    }

public:
    UnixNanotime get_creation_time()
    {
        if (creation_time == UnixNanotime::uninitialized())
            get_file_times();
        return creation_time;
    }
    UnixNanotime get_last_write_time()
    {
        if (last_write_time == UnixNanotime::uninitialized())
            get_file_times();
        return last_write_time;
    }

    int64_t get_file_size()
    {
        if (file_size == -1) {
            if (!is_valid())
                throw AttemptToGetFileSizeOfAClosedFile();

            if (GetFileSizeEx(handle, (PLARGE_INTEGER)&file_size) == 0)
                file_size = -2;
        }
        return file_size;
    }
#else
    void get_last_write_time_and_file_size()
    {
        if (!is_valid())
            throw AttemptToGetTimeOfAClosedFile();

        struct stat st;
        if (fstat(fd, &st) != 0)
            throw FStatFailed();

        last_write_time = UnixNanotime::from_nanotime_t(st.st_mtim.tv_sec * 1000000000 + st.st_mtim.tv_nsec);
        file_size = S_ISREG(st.st_mode) ? st.st_size : -2;
    }

public:
    UnixNanotime get_creation_time()
    {
        throw GetCreationTimeIsNotImplemented();
    }
    UnixNanotime get_last_write_time()
    {
        if (last_write_time == UnixNanotime::uninitialized())
            get_last_write_time_and_file_size();
        return last_write_time;
    }

    int64_t get_file_size()
    {
        if (file_size == -1)
            get_last_write_time_and_file_size();
        return file_size;
    }
#endif
};
}
