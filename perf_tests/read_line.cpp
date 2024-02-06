#include "common.hpp"

std::string read_line_via_read_byte(IFile &f)
{
    std::string r;
    do {
        char c = (char)f.read_byte();
        if (c == '\n') {
            if (!r.empty() && r.back() == '\r')
                r.pop_back();
            break;
        }
        r.append(1, c);
    } while (!f.at_eof());
    return r;
}

int main()
{
    struct stat s;
    if (stat("unixdict.txt", &s) != 0) {
        printf("Please download unixdict.txt[http://wiki.puzzlers.org/pub/wordlists/unixdict.txt]");
        return 1;
    }

    tests_count = 50;

    test_ffh([](IFile &f) {
        uint32_t words_count = 0, total_len = 0;
        while (!f.at_eof()) {
            words_count++;
            total_len += f.read_line().length();
        }
        return total_len * 1000 / words_count;
    }, nullptr, "unixdict.txt");

    test_ffh([](IFile &f) {
        uint32_t words_count = 0, total_len = 0;
        while (!f.at_eof()) {
            words_count++;
            total_len += read_line_via_read_byte(f).length();
        }
        return total_len * 1000 / words_count;
    }, "via read_byte()", "unixdict.txt");

    test_ffh([](IFile &f) {
        f.set_buffer_size(4 * 1024);
        uint32_t words_count = 0, total_len = 0;
        while (!f.at_eof()) {
            words_count++;
            total_len += f.read_line().length();
        }
        return total_len * 1000 / words_count;
    }, "with 4KiB buffer", "unixdict.txt");

    test_c([](FILE *f) {
        uint32_t words_count = 0, total_len = 0;
        char s[32];
        while (fgets(s, sizeof(s), f)) {
            size_t len = strlen(s);
            if (len > 0 && s[len - 1] == '\n')
                len--;
            words_count++;
            total_len += len;
        }
        return total_len * 1000 / words_count;
    }, nullptr, "unixdict.txt");

    test_c([](FILE *f) {
        uint32_t words_count = 0, total_len = 0;
        char s[32];
        while (!at_eof_c(f)) {
            fgets(s, sizeof(s), f);
            size_t len = strlen(s);
            if (len > 0 && s[len - 1] == '\n')
                len--;
            words_count++;
            total_len += len;
        }
        return total_len * 1000 / words_count;
    }, "with at_eof() simulation", "unixdict.txt");

    test_cpp([](std::ifstream &f) {
        uint32_t words_count = 0, total_len = 0;
        std::string line;
        while (std::getline(f, line)) {
            words_count++;
            total_len += line.length();
        }
        return total_len * 1000 / words_count;
    }, nullptr, "unixdict.txt");

    test_cpp([](std::ifstream &f) {
        uint32_t words_count = 0, total_len = 0;
        while (!at_eof_cpp_unget(f)) {
            std::string line;
            std::getline(f, line);
            words_count++;
            total_len += line.length();
        }
        return total_len * 1000 / words_count;
    }, "with at_eof() simulation via unget()", "unixdict.txt");

    test_cpp([](std::ifstream &f) {
        uint32_t words_count = 0, total_len = 0;
        while (!at_eof_cpp_peek(f)) {
            std::string line;
            std::getline(f, line);
            words_count++;
            total_len += line.length();
        }
        return total_len * 1000 / words_count;
    }, "with at_eof() simulation via peek()", "unixdict.txt");
}
