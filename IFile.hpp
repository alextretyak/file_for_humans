#pragma once
#include <vector>
#include <cstdint> // for uint8_t
#include "FileHandle.hpp"
#include <memory> // for std::unique_ptr
#include <string.h> // for memcmp and memchr [GCC]
#include <assert.h>

const size_t IFILE_DEFAULT_BUFFER_SIZE = 32*1024;

class IFileBufferAlreadyAllocated {};
class UnexpectedEOF {};

/*
H‘Naming things is hard’

`IFile` is not a very good name, as `I` could mean ‘interface’ (see [https://en.wikipedia.org/wiki/IUnknown]).
I thought of using `FileReader` or `BufferedFileReader`, but `FileReader` means something different in Java and JavaScript.
There are `CInFile` and `COutFile` here[https://android.googlesource.com/platform/external/lzma/+/kitkat-dev/CPP/Windows/FileIO.h <- google:‘"class CInFile"’],
so you can think of `IFile` and `OFile` as short forms of them.
*/
class IFile
{
    detail::FileHandle<true> fh;
    std::unique_ptr<uint8_t[]> buffer;
    size_t buffer_pos = 0, buffer_size = 0, buffer_capacity = IFILE_DEFAULT_BUFFER_SIZE;
    uint64_t file_pos_of_buffer_start = 0;
    bool is_eof_reached = false;
    bool eof_indicator = false; // >[https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf <- https://en.wikipedia.org/wiki/C23_(C_standard_revision)]:‘The `feof` function tests the end-of-file indicator’

    NOINLINE bool has_no_data_left()
    {
        assert(buffer_pos == buffer_size); // make sure there is no available data in the buffer

        if (is_eof_reached) // check to prevent extra `read()` syscalls
            return true;

        if (buffer == nullptr)
            buffer.reset(new uint8_t[buffer_capacity]);

        file_pos_of_buffer_start += buffer_size;
        buffer_size = fh.read(buffer.get(), buffer_capacity);
        if (buffer_size < buffer_capacity)
            is_eof_reached = true;

        buffer_pos = 0;

        return buffer_size == 0;
    }

    static bool is_bom(const uint8_t *p)
    {
        uint8_t utf8bom[3] = {0xEF, 0xBB, 0xBF};
        return memcmp(p, utf8bom, 3) == 0;
    }

public:
    template <class... Args> IFile(Args&&... args) : fh(std::forward<Args>(args)...) {}
    template <class... Args> bool open(Args&&... args) {return fh.open(std::forward<Args>(args)...);}
    ~IFile() {fh.close();}
    void close()
    {
        fh.close();
        buffer_pos = 0;
        buffer_size = 0;
        file_pos_of_buffer_start = 0;
        is_eof_reached = false;
        eof_indicator = false;
    }

    void set_buffer_size(size_t sz)
    {
        if (buffer != nullptr)
            throw IFileBufferAlreadyAllocated();
        buffer_capacity = sz;
    }

    /*
    `at_eof()` function works like `eof()` in Pascal, i.e. it returns true if the file is at the end.
    The name `at_the_end()` for this function has been rejected as it is longer, and the name `at_eof()` is still quite correct:
    >[http://www.irietools.com/iriepascal/progref177.html <- google:‘pascal eof’][-1]:‘file is at end-of-file’
    */
    bool at_eof()
    {
        if (buffer_pos < buffer_size)
            return false;
        return has_no_data_left();
    }

    /*
    `eof_passed()` function works like `eof()` in C, i.e. it returns true if a read operation has attempted to read past the end of the file.
    The name of this function was inspired by:
    >[https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/feof]:‘the end of `stream` has been passed.’
    The name `eof_reached()` for this function has been rejected because it is confusing:
    >[-1]:‘The *‘eof’ function is used to determine when the end of the text file has been reached.’
    */
    bool eof_passed()
    {
        return eof_indicator;
    }

    uint8_t read_byte()
    {
        if (at_eof())
            throw UnexpectedEOF();
        return buffer[buffer_pos++];
    }

    std::string read_text() // reads whole file and returns its contents as a string; only works if the file pointer is at the beginning of the file (`read_text_to_end()` has no such limitation)
    {
    }

    std::string read_text_to_end() // the method name was inspired by [https://doc.rust-lang.org/std/io/trait.Read.html#method.read_to_end]
    {
    }

    std::string read_until(char delim, bool keep_delim = false)
    {
        if (at_eof())
            throw UnexpectedEOF();

        // Skip the BOM at the beginning of the file, if present
        if (file_pos_of_buffer_start == 0 && buffer_pos == 0)
            if (buffer_size >= 3)
                if (is_bom(buffer.get())) {
                    if (buffer_size == 3)
                        throw UnexpectedEOF();
                    buffer_pos = 3;
                }

        // Scan buffer for delim
        if (uint8_t *p = (uint8_t*)memchr(buffer.get() + buffer_pos, delim, buffer_size - buffer_pos)) {
            size_t res_size = p - (buffer.get() + buffer_pos);
            std::string res((char*)buffer.get() + buffer_pos, res_size + int(keep_delim));
            buffer_pos += res_size + 1;
            return res;
        }

        std::string res((char*)buffer.get() + buffer_pos, buffer_size - buffer_pos);
        buffer_pos = buffer_size;
        while (!has_no_data_left()) {
            if (uint8_t *p = (uint8_t*)memchr(buffer.get(), delim, buffer_size)) {
                res.append((char*)buffer.get(), p - buffer.get() + int(keep_delim));
                buffer_pos += p - buffer.get() + 1;
                return res;
            }
            res.append((char*)buffer.get(), buffer_size);
            buffer_pos = buffer_size;
        }
        return res;
    }

    std::string read_line(bool keep_newline = false)
    {
        std::string r = read_until('\n', keep_newline);
        if (!keep_newline) {
            if (!r.empty() && r.back() == '\r')
                r.pop_back();
        }
        else
            if (r.back() == '\n' && r.length() >= 2 && r[r.length() - 2] == '\r')
                r.erase(r.length() - 2, 1);
        return r;
    }

    std::string read_line_reae(bool keep_newline = false) // ‘r’‘e’‘a’‘e’ means ‘r’eturns ‘e’mpty [string] (‘a’t ‘e’nd of file);
    {                                                     // `read_line_reae()` corresponds to `std::getline()` in C++,
        if (at_eof()) {                                   // `read_line_reae(true)` corresponds to `readline()` in Python and to `read_line()` in Rust.
            eof_indicator = true;
            return std::string(); // when the EOF is reached, this function returns an empty string instead of throwing an UnexpectedEOF exception
        }

        // Skip the BOM at the beginning of the file, if present
        if (file_pos_of_buffer_start == 0 && buffer_pos == 0)
            if (buffer_size >= 3)
                if (is_bom(buffer.get())) {
                    if (buffer_size == 3) {
                        eof_indicator = true;
                        return std::string(); // when the EOF is reached, this function returns an empty string instead of throwing an UnexpectedEOF exception
                    }
                    buffer_pos = 3;
                }

        std::string r = read_until('\n', true);
        assert(!r.empty()); // the above code guarantees that `r` cannot be empty here
        if (r.back() != '\n') {
            assert(is_eof_reached && buffer_pos == buffer_size);
            eof_indicator = true;
            return r;
        }
        assert(r.back() == '\n');
        if (!keep_newline) {
            r.pop_back();
            if (!r.empty() && r.back() == '\r')
                r.pop_back();
        }
        else
            if (r.length() >= 2 && r[r.length() - 2] == '\r')
                r.erase(r.length() - 2, 1);
        return r;
    }

    std::vector<uint8_t> read_bytes()
    {
    }

    std::vector<uint8_t> read_bytes_to_end()
    {
    }

    std::vector<uint8_t> read_bytes_at_most()
    {
    }

    void read_bytes(uint8_t *p, size_t count)
    {
        if (at_eof())
            throw UnexpectedEOF();

        if (count > buffer_capacity) { // optimize large reads (avoid extra `read()` syscalls)
            // First of all, copy all of the remaining bytes in the buffer
            size_t n = buffer_size - buffer_pos;
            assert(count >= n);
            memcpy(p, buffer.get() + buffer_pos, n);
            count -= n;
            p += n;

            // Read the rest in a single [or at least a minimal number of] `read()` syscall(s)
            if (fh.read(p, count) < count)
                throw UnexpectedEOF();

            file_pos_of_buffer_start += buffer_size + count;
            buffer_pos = buffer_size = 0;
            return;
        }

        while (true) {
            size_t n = (std::min)(buffer_size - buffer_pos, count);
            memcpy(p, buffer.get() + buffer_pos, n);
            buffer_pos += n;
            count -= n;
            if (count == 0)
                return;
            if (has_no_data_left())
                throw UnexpectedEOF();
            p += n;
        }
    }

    template <typename Struct> void read_struct(Struct &s)
    {
        read_bytes((uint8_t*)&s, sizeof(Struct));
    }
};
