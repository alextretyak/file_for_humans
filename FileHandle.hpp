#ifdef __GNUC__
#define NOINLINE __attribute__((noinline))
#elif _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

namespace detail
{
#ifdef _WIN32
// [https://stackoverflow.com/questions/30829364/open-utf8-encoded-filename-in-c-windows <- google:‘c++ filesystem utf8 filename’]
std::u16string ToUtf16(const char *s, size_t l)
{
    std::u16string ret;
    int len = MultiByteToWideChar(CP_UTF8, 0, s, l, NULL, 0);
    if (len > 0)
    {
        ret.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, s, l, (wchar_t*)&ret[0], len);
    }
    return ret;
}
#endif
}

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
    FileHandle(const char *s, size_t len) : FileHandle(ToUtf16(s, len)) {}
    FileHandle(const char16_t *s, size_t len) : handle(INVALID_HANDLE_VALUE) {if (!open(s, len)) throw FileOpenError();}
    FileHandle(const char16_t *s) : handle(INVALID_HANDLE_VALUE) {if (!open(s)) throw FileOpenError();}
    FileHandle(const char *s) : FileHandle(s, strlen(s)) {}

    bool open(const char *s, size_t len) {return open(ToUtf16(s, len));}
    bool open(const char *s)             {return open(s, strlen(s));}
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

        DWORD numberOfBytesRead;
        if (!ReadFile(handle, buf, sz, &numberOfBytesRead, NULL))
            throw IOError();
        return numberOfBytesRead;
    }

    void close()
    {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
#endif

    ~FileHandle() {close();}
};
}
