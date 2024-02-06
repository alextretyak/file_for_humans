#include "common.hpp"

int main()
{
    TestDataFileMaker _tdfm_;

    test_ffh([](IFile &f) {
        uint32_t r = 0;
        while (!f.at_eof())
            r += f.read_byte();
        return r;
    });

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
}
