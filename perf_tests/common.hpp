#include <chrono>
#include <stdio.h>
#include <float.h> // for DBL_MAX in GCC
#include <fstream>
#include "../IFile.hpp"

class HRCTimePoint
{
    std::chrono::high_resolution_clock::time_point tp;
public:
    HRCTimePoint(const std::chrono::high_resolution_clock::time_point &tp) : tp(tp) {}

    double operator-(const HRCTimePoint &tp2) const
    {
        return std::chrono::duration_cast<std::chrono::duration<double>>(tp - tp2.tp).count();
    }
};

HRCTimePoint perf_counter()
{
    return std::chrono::high_resolution_clock::now();
}

bool at_eof_c(FILE *f)
{
    int c = fgetc(f);
    ungetc(c, f);
    return c == EOF;
}

bool at_eof_cpp_unget(std::ifstream &f)
{
    int c = f.get();
    if (c == std::ifstream::traits_type::eof())
        return true;
    f.unget();
    return false;
}

bool at_eof_cpp_peek(std::ifstream &f)
{
    return f.peek() == std::ifstream::traits_type::eof();
}

//void make_test_data_file()
class TestDataFileMaker
{
public:
    TestDataFileMaker()
    {
        std::ofstream f("test.dat", std::ios::binary);
        for (int i = 0; i < 1024*1024; i++)
            f.put(rand() & 0xFF);
    }
    ~TestDataFileMaker()
    {
        remove("test.dat");
    }
};

double ffh_time;
int tests_count = 10;

template <class Body> void test(const char *test_base_name, Body &&body, const char *test_name)
{
    double time = DBL_MAX;
    uint32_t r;
    for (int t = 0; t < tests_count; t++)
        body(r, time);

    printf("\n");
    if (test_name != nullptr)
        printf("--- %s %s ---\n", test_base_name, test_name);
    else
        printf("--- %s ---\n", test_base_name);
    printf("                                          result: %u\n", r);
    printf("time: %.3f\n", time);
    if (strcmp(test_base_name, "ffh") == 0 && test_name == nullptr)
        ffh_time = time;
    else
        printf("times slower than ffh: %.2f\n", time / ffh_time);
}

template <class Func> void test_ffh(Func &&func, const char *test_name = nullptr, const char *test_file_name = "test.dat")
{
    test("ffh", [&](uint32_t &r, double &time) {
        IFile f(test_file_name);
        auto start = perf_counter();
        r = func(f);
        time = (std::min)(time, perf_counter() - start);
    }, test_name);
}

template <class Func> void test_c(Func &&func, const char *test_name = nullptr, const char *test_file_name = "test.dat")
{
    test("C", [&](uint32_t &r, double &time) {
        FILE *f = fopen(test_file_name, "rb");
        auto start = perf_counter();
        r = func(f);
        time = (std::min)(time, perf_counter() - start);
        fclose(f);
    }, test_name);
}

template <class Func> void test_cpp(Func &&func, const char *test_name = nullptr, const char *test_file_name = "test.dat")
{
    test("C++", [&](uint32_t &r, double &time) {
        std::ifstream f(test_file_name, std::ios::binary);
        auto start = perf_counter();
        r = func(f);
        time = (std::min)(time, perf_counter() - start);
    }, test_name);
}
