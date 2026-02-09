#include "common.hpp"

template <int struct_size> void test_struct_size()
{
    printf("%2i:", struct_size);
    for (int via_read_byte = 0; via_read_byte <= 1; via_read_byte++) {
        double time = DBL_MAX;
        for (int t = 0; t < tests_count; t++) {
            IFile f("test.dat");
            auto start = perf_counter();
            uint8_t struct_data[struct_size];
            if (via_read_byte) {
                while (!f.at_eof())
                    for (int i = 0; i < struct_size; i++)
                        struct_data[i] = f.read_byte();
            }
            else {
                while (!f.at_eof())
                    f.read_bytes<struct_size>(struct_data);
            }
            time = (std::min)(time, perf_counter() - start);
        }
        printf(" %.3f", time);
    }
    printf("\n");

    test_struct_size<struct_size/2>();
}

template <> void test_struct_size<0>()
{
}

int main()
{
    TestDataFileMaker _tdfm_(64*1024*1024);

    test_struct_size<32>();
}
