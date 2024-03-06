#include "common.hpp"
#ifdef _WIN32
#define getc_unlocked _getc_nolock
#endif

int main()
{
    TestDataFileMaker _tdfm_;

    test_ffh([](IFile &f) {
        uint32_t r = 0;
        while (!f.at_eof())
            r += f.read_byte();
        return r;
    });

    test_ffh([](IFile &f) {
        f.set_buffer_size(4 * 1024);
        uint32_t r = 0;
        while (!f.at_eof())
            r += f.read_byte();
        return r;
    }, "with 4KiB buffer");

    test_c([](FILE *f) {
        uint32_t r = 0;
        for (int c; (c = fgetc(f)) != EOF;)
            r += c;
        return r;
    });

    test_cpp([](std::ifstream &f) {
        uint32_t r = 0;
        for (int c; (c = f.get()) != std::ifstream::traits_type::eof();)
            r += c;
        return r;
    });

    test_c([](FILE *f) {
        uint32_t r = 0;
        while (!at_eof_c(f))
            r += fgetc(f);
        return r;
    }, "with at_eof() simulation");

    test_cpp([](std::ifstream &f) {
        uint32_t r = 0;
        while (!at_eof_cpp_unget(f))
            r += f.get();
        return r;
    }, "with at_eof() simulation via unget()");

    test_cpp([](std::ifstream &f) {
        uint32_t r = 0;
        while (!at_eof_cpp_peek(f))
            r += f.get();
        return r;
    }, "with at_eof() simulation via peek()");

    test_c([](FILE *f) {
        setvbuf(f, NULL, _IOFBF, 32 * 1024);
        uint32_t r = 0;
        for (int c; (c = fgetc(f)) != EOF;)
            r += c;
        return r;
    }, "with 32KiB buffer");

    test_c([](FILE *f) {
        setvbuf(f, NULL, _IOFBF, 1024 * 1024);
        uint32_t r = 0;
        for (int c; (c = fgetc(f)) != EOF;)
            r += c;
        return r;
    }, "with 1MiB buffer");

    test_c([](FILE *f) {
        setvbuf(f, NULL, _IOFBF, 256);
        uint32_t r = 0;
        for (int c; (c = fgetc(f)) != EOF;)
            r += c;
        return r;
    }, "with 256B buffer");

    test_cpp([](std::ifstream &f) {
        static char buffer[32 * 1024];
        f.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
        uint32_t r = 0;
        for (int c; (c = f.get()) != std::ifstream::traits_type::eof();)
            r += c;
        return r;
    }, "with 32KiB buffer");

    test_c([](FILE *f) {
        uint32_t r = 0;
        for (int c; (c = getc_unlocked(f)) != EOF;)
            r += c;
        return r;
    }, "getc_unlocked");

    test_c([](FILE *f) {
        setvbuf(f, NULL, _IOFBF, 32 * 1024);
        uint32_t r = 0;
        for (int c; (c = getc_unlocked(f)) != EOF;)
            r += c;
        return r;
    }, "getc_unlocked with 32KiB buffer");
}
