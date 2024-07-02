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
#if __has_include (<sys/syscall.h>) && __has_include (<linux/stat.h>) // for `statx`
    // [https://github.com/boostorg/filesystem/blob/master/config/has_statx_syscall.cpp]
    #include <sys/syscall.h> // for __NR_statx
    #include <linux/stat.h>  // for `struct statx` and `STATX_BTIME`
#endif
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
class AssignNonStdHandle {};
class WrongFileNameStr {};
class FileIsAlreadyOpened {};
class AttemptToReadAClosedFile {};
class AttemptToWriteAClosedFile {};
class IOError {};
class AttemptToGetTimeOfAClosedFile {};
class AttemptToSetTimeOfAClosedFile {};
class GetFileTimeFailed {};
class GetCreationTimeIsNotSupported {};
class FStatFailed {};
class StatXFailed {};
class AttemptToGetFileSizeOfAClosedFile {};
class SeekFailed {};
class SetLastWriteTimeFailed {};

namespace detail
{
#ifdef _WIN32
inline HANDLE  stdin_handle() {static HANDLE h = GetStdHandle(STD_INPUT_HANDLE ); return h;}
inline HANDLE stdout_handle() {static HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE); return h;}
inline HANDLE stderr_handle() {static HANDLE h = GetStdHandle(STD_ERROR_HANDLE ); return h;}
typedef HANDLE HandleType;
#else
inline int  stdin_handle() {return  STDIN_FILENO;}
inline int stdout_handle() {return STDOUT_FILENO;}
inline int stderr_handle() {return STDERR_FILENO;}
typedef int HandleType;
#endif

template <bool for_reading> bool is_std_handle(HandleType);
template <> inline bool is_std_handle<true> (HandleType handle) {return handle ==  stdin_handle();}
template <> inline bool is_std_handle<false>(HandleType handle) {return handle == stdout_handle() || handle == stderr_handle();}

template <bool for_reading> class FileHandle
{
public:
    // Redirecting common constructors
    FileHandle(const std::string    &s, bool append = false) : FileHandle(s.c_str(), s.length(), append) {}
    FileHandle(const std::u16string &s, bool append = false) : FileHandle(s.c_str(), s.length(), append) {}

    bool  open(const std::string    &s, bool append = false) {return open(s.c_str(), s.length(), append);}
    bool  open(const std::u16string &s, bool append = false) {return open(s.c_str(), s.length(), append);}

#ifdef _WIN32
    UniqueHandle<HANDLE, INVALID_HANDLE_VALUE> handle;

    FileHandle() {}
    FileHandle(const char *s, size_t len, bool append = false) : FileHandle(utf::as_u16(utf::std::string_view(s, len)), append) {}
    FileHandle(const char16_t *s, size_t len, bool append = false) {if (!open(s, len, append)) throw FileOpenError();}
    FileHandle(const char16_t *s, bool append = false) {if (!open(s, append)) throw FileOpenError();}
    FileHandle(const char *s, bool append = false) : FileHandle(utf::as_u16(s), append) {}

    void assign_std_handle(HANDLE h)
    {
        handle = h;
        if (!is_std_handle())
            throw AssignNonStdHandle();
    }
    void assign_std_handle(const FileHandle &fh) {assign_std_handle(fh.handle);}

    bool open(const char *s, size_t len, bool append = false) {return open(utf::as_u16(utf::std::string_view(s, len)), append);}
    bool open(const char *s,             bool append = false) {return open(utf::as_u16(s), append);}
    bool open(const char16_t *s, size_t len, bool append = false)
    {
        if (s[len] != 0)
            throw WrongFileNameStr();
        return open(s, append);
    }

    bool open(const char16_t *s, bool append = false)
    {
        if (handle != INVALID_HANDLE_VALUE)
            throw FileIsAlreadyOpened();

        if (for_reading)
            handle = CreateFileW((wchar_t*)s, GENERIC_READ , FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        else {
            handle = CreateFileW((wchar_t*)s, GENERIC_WRITE, 0, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (handle == INVALID_HANDLE_VALUE)
                return false;
            if (append) {
                LARGE_INTEGER liDistanceToMove = {0};
                if (!SetFilePointerEx(handle, liDistanceToMove, NULL, FILE_END))
                    throw SeekFailed();
            }
        }
        return handle != INVALID_HANDLE_VALUE;
    }

    bool is_associated_with_console() const {return GetFileType(handle) == FILE_TYPE_CHAR;} // >[https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/isatty]:‘is associated with a character device (a terminal, console, ...)’

    bool is_valid() {return handle != INVALID_HANDLE_VALUE;}

private:
    DWORD ReadFileAtPos(void *buf, DWORD sz, int64_t pos)
    {
        DWORD numberOfBytesRead;
        if (pos == -1) {
            if (!ReadFile(handle, buf, sz, &numberOfBytesRead, NULL))
                throw IOError();
        }
        else {
            OVERLAPPED ovp = {0};
            ovp.Offset     = pos & 0xFFFFFFFF;
            ovp.OffsetHigh = uint64_t(pos) >> 32;
            if (!ReadFile(handle, buf, sz, &numberOfBytesRead, &ovp)) {
                if (GetLastError() != ERROR_HANDLE_EOF)
                    throw IOError();
                else if (pos > get_file_size())
                    throw SeekFailed();
            }
        }
        return numberOfBytesRead;
    }
public:
    size_t read(void *buf, size_t sz, int64_t pos = -1)
    {
        if (handle == INVALID_HANDLE_VALUE)
            throw AttemptToReadAClosedFile();

        if (sz <= 0xFFFFFFFFu) {
            return ReadFileAtPos(buf, (DWORD)sz, pos);
        }
        else {
            char *b = (char*)buf;
            while (true) {
                DWORD numberOfBytesRead = ReadFileAtPos(b, (DWORD)(std::min)(sz, (size_t)0xFFFF0000), pos);
                pos = -1;
                if (numberOfBytesRead == 0)
                    return b - (char*)buf;
                b += numberOfBytesRead;
                sz -= numberOfBytesRead;
                if (sz == 0)
                    return b - (char*)buf;
            }
        }
    }

    void write(const void *buf, size_t sz)
    {
        if (handle == INVALID_HANDLE_VALUE)
            throw AttemptToWriteAClosedFile();

        DWORD numberOfBytesWritten;
        if (!WriteFile(handle, buf, (DWORD)sz, &numberOfBytesWritten, NULL) || numberOfBytesWritten != sz)
            throw IOError();
    }

    void seek(int64_t pos)
    {
        static_assert(!for_reading, "seek() is only allowed when writing");

        LARGE_INTEGER liDistanceToMove;
        liDistanceToMove.QuadPart = pos;
        if (!SetFilePointerEx(handle, liDistanceToMove, NULL, FILE_BEGIN))
            throw SeekFailed();
    }

    bool is_std_handle() const {return detail::is_std_handle<for_reading>(handle);}

    void close()
    {
        if (is_std_handle())
            handle = INVALID_HANDLE_VALUE;
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
#else
    UniqueHandle<int, -1> fd;

    FileHandle() {}
    FileHandle(const char *s, bool append = false) {if (!open(s, append)) throw FileOpenError();}
    FileHandle(const char *s, size_t len, bool append = false) {if (!open(s, len, append)) throw FileOpenError();}
    FileHandle(const char16_t *s, bool append = false) : FileHandle(utf::as_str8(s), append) {}
    FileHandle(const char16_t *s, size_t len, bool append = false) : FileHandle(utf::as_str8(utf::std::u16string_view(s, len)), append) {}

    void assign_std_handle(int d)
    {
        fd = d;
        if (!is_std_handle())
            throw AssignNonStdHandle();
    }
    void assign_std_handle(const FileHandle &fh) {assign_std_handle(fh.fd);}

    bool open(const char16_t *s, size_t len, bool append = false) {return open(utf::as_str8(utf::std::u16string_view(s, len)), append);}
    bool open(const char16_t *s,             bool append = false) {return open(utf::as_str8(s), append);}
    bool open(const char *s, size_t len, bool append = false)
    {
        if (s[len] != 0)
            throw WrongFileNameStr();
        return open(s, append);
    }

    bool open(const char *s, bool append = false)
    {
        if (fd != -1)
            throw FileIsAlreadyOpened();

        if (for_reading)
            fd = ::open(s, O_RDONLY);
        else
            fd = ::open(s, O_WRONLY|O_CREAT|(append ? O_APPEND : O_TRUNC), 0666);
        return fd != -1;
    }

    bool is_associated_with_console() const {return isatty(fd) != 0;}

    bool is_valid() {return fd != -1;}

    size_t read(void *buf, size_t sz, int64_t pos = -1)
    {
        if (fd == -1)
            throw AttemptToReadAClosedFile();

        if (pos != -1)
            if (lseek(fd, pos, SEEK_SET) != pos || pos > get_file_size()) // the `lseek()` return value is not reliable, so there is an additional check
                throw SeekFailed();

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

    void write(const void *buf, size_t sz)
    {
        if (fd == -1)
            throw AttemptToWriteAClosedFile();

        char *b = (char*)buf;
        while (sz != 0) {
            ssize_t r = ::write(fd, b, std::min(sz, (size_t)0x7ffff000)); // On Linux, write() will transfer at most 0x7ffff000 bytes
            if (r == -1 || r == 0)
                throw IOError();
            b += r;
            sz -= r;
        }
    }

    void seek(int64_t pos)
    {
        static_assert(!for_reading, "seek() is only allowed when writing");

        if (lseek(fd, pos, SEEK_SET) != pos)
            throw SeekFailed();
    }

    bool is_std_handle() const {return detail::is_std_handle<for_reading>(fd);}

    void close()
    {
        if (is_std_handle())
            fd = -1;
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
    }
#endif
#if !defined(_MSC_VER) || _MSC_VER > 1800
    FileHandle(FileHandle &&) = default;
#else // unfortunately, MSVC 2013 doesn't support defaulted move constructors
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

    void set_last_write_time(UnixNanotime t)
    {
        if (!is_valid())
            throw AttemptToSetTimeOfAClosedFile();

        uint64_t filetime = t.to_uint64<100, _1601_TO_1970>();
        if (SetFileTime(handle, NULL, NULL, (FILETIME*)&filetime) == 0)
            throw SetLastWriteTimeFailed();
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
#ifndef __NR_statx
        throw GetCreationTimeIsNotSupported();
#else
        if (creation_time == UnixNanotime::uninitialized()) {
            if (!is_valid())
                throw AttemptToGetTimeOfAClosedFile();

            struct statx st;
            // Avoid direct use of `statx()` [appeared in Linux 4.11, glibc 2.28] to support Ubuntu 18.04 [Linux 4.15, glibc 2.27]
            if (syscall(__NR_statx, (int)fd, "", AT_EMPTY_PATH, STATX_BTIME, &st) != 0 || !(st.stx_mask & STATX_BTIME))
                throw StatXFailed();
            creation_time = UnixNanotime::from_nanotime_t(st.stx_btime.tv_sec * 1000000000 + st.stx_btime.tv_nsec);
        }
        return creation_time;
#endif
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

    void set_last_write_time(UnixNanotime t)
    {
        if (!is_valid())
            throw AttemptToSetTimeOfAClosedFile();

        timespec times[2];
        times[0].tv_nsec = UTIME_OMIT;
        t.to_timespec(times[1]);
        if (futimens(fd, times) != 0)
            throw SetLastWriteTimeFailed();
    }
#endif
};
}
