#include "common.hpp"

int main()
{
    TestDataFileMaker _tdfm_;

    test_ffh([](IFile &f) {
        uint32_t r = 0;
        while (!f.at_eof()) {
            uint32_t d;
            f.read_struct(d);
            r += d;
        }
        return r;
    });

    test_ffh([](IFile &f) {
        f.set_buffer_size(4 * 1024);
        uint32_t r = 0;
        while (!f.at_eof()) {
            uint32_t d;
            f.read_struct(d);
            r += d;
        }
        return r;
    }, "with 4KiB buffer");

    test_c([](FILE *f) {
        uint32_t r = 0, d;
        while (fread(&d, 4, 1, f) == 1)
            r += d;
        return r;
    });

    test_cpp([](std::ifstream &f) {
        uint32_t r = 0, d;
        while (f.read((char*)&d, 4))
            r += d;
        return r;
    });

    test_c([](FILE *f) {
        uint32_t r = 0;
        while (!at_eof_c(f)) {
            uint32_t d;
            fread(&d, 4, 1, f);
            r += d;
        }
        return r;
    }, "with at_eof() simulation");

    test_cpp([](std::ifstream &f) {
        uint32_t r = 0;
        while (!at_eof_cpp_unget(f)) {
            uint32_t d;
            f.read((char*)&d, 4);
            r += d;
        }
        return r;
    }, "with at_eof() simulation via unget()");

    test_cpp([](std::ifstream &f) {
        uint32_t r = 0;
        while (!at_eof_cpp_peek(f)) {
            uint32_t d;
            f.read((char*)&d, 4);
            r += d;
        }
        return r;
    }, "with at_eof() simulation via peek()");
}
