#pragma once
#include <string>
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h> // for `open()`
#include <unistd.h> // for `read()`
#endif
#include "utf.hpp"

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
HANDLE handle;
    FileHandle(FileHandle &&fh) : handle(fh.handle) {fh.handle = INVALID_HANDLE_VALUE;}
    FileHandle &operator=(FileHandle &&fh)
    {
        close();
        handle = fh.handle;
        fh.handle = INVALID_HANDLE_VALUE;
        return *this;
    }

    FileHandle() : handle(INVALID_HANDLE_VALUE) {}
    FileHandle(const char *s, size_t len) : FileHandle(utf::as_u16(utf::std::string_view(s, len))) {}
    FileHandle(const char16_t *s, size_t len) : handle(INVALID_HANDLE_VALUE) {if (!open(s, len)) throw FileOpenError();}
    FileHandle(const char16_t *s) : handle(INVALID_HANDLE_VALUE) {if (!open(s)) throw FileOpenError();}
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
    int fd;
    FileHandle(FileHandle &&fh) : fd(fh.fd) {fh.fd = -1;}
    FileHandle &operator=(FileHandle &&fh)
    {
        close();
        fd = fh.fd;
        fh.fd = -1;
        return *this;
    }

    FileHandle() : fd(-1) {}
    FileHandle(const char *s) : fd(-1) {if (!open(s)) throw FileOpenError();}
    FileHandle(const char *s, size_t len) : fd(-1) {if (!open(s, len)) throw FileOpenError();}
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

    ~FileHandle() {close();}
};
}
