#include <chrono>
#include <stdio.h>
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
            f.put(rand() % 0xFF);
    }
    ~TestDataFileMaker()
    {
        remove("test.dat");
    }
};

double ffh_time;

template <class Func> void test_ffh(Func &&func)
{
    double time = DBL_MAX;
    uint32_t r;
    for (int t = 0; t < 10; t++) {
        IFile f("test.dat");
        auto start = perf_counter();
        r = func(f);
        time = (std::min)(time, perf_counter() - start);
    }
    ffh_time = time;
    printf("--- ffh ---\n");
    printf("                                          result: %u\n", r);
    printf("time: %.3f\n", time);
}

template <class Func> void test_c(Func &&func, const char *test_name = nullptr)
{
    double time = DBL_MAX;
    uint32_t r;
    for (int t = 0; t < 10; t++) {
        FILE *f = fopen("test.dat", "rb");
        auto start = perf_counter();
        r = func(f);
        time = (std::min)(time, perf_counter() - start);
        fclose(f);
    }
    printf("\n");
    if (test_name != nullptr)
        printf("--- C (%s) ---\n", test_name);
    else
        printf("--- C ---\n");
    printf("                                          result: %u\n", r);
    printf("time: %.3f\n", time);
    printf("times slower than ffh: %.2f\n", time / ffh_time);
}

template <class Func> void test_cpp(Func &&func, const char *test_name = nullptr)
{
    double time = DBL_MAX;
    uint32_t r;
    for (int t = 0; t < 10; t++) {
        std::ifstream f("test.dat", std::ios::binary);
        auto start = perf_counter();
        r = func(f);
        time = (std::min)(time, perf_counter() - start);
    }
    printf("\n");
    if (test_name != nullptr)
        printf("--- C++ (%s) ---\n", test_name);
    else
        printf("--- C++ ---\n");
    printf("                                          result: %u\n", r);
    printf("time: %.3f\n", time);
    printf("times slower than ffh: %.2f\n", time / ffh_time);
}
