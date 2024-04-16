#pragma once
#include <vector>
#include <array>
#include <cstdint> // for uint8_t
#include "FileHandle.hpp"
#include <memory> // for std::unique_ptr
#include <string.h> // for memcmp and memchr [GCC]
#ifndef assert
#include <assert.h>
#endif

const size_t IFILE_DEFAULT_BUFFER_SIZE = 32*1024;
const size_t IFILE_BUFFER_SIZE_RIGHT_AFTER_SEEK = 4*1024;

class IFileBufferAlreadyAllocated {};
class UnexpectedEOF {};
class StartsWithMustBeCalledAtTheBeginningOfTheFile {};
class IFileUnicodeDecodeError {};
class ReadTextMustBeCalledAtTheBeginningOfTheFile {};
class ReadBytesMustBeCalledAtTheBeginningOfTheFile {};
class FileIsTooLargeToFitInMemory {};
class OSReportedIncorrectFileSize {};
class FileSizeIsUnknown {};
class FileDoesNotSupportPositioning {};

/*
H‘Naming things is hard’

`IFile` is not a very good name, as `I` could mean ‘interface’ (see [https://en.wikipedia.org/wiki/IUnknown]).
I thought of using `FileReader` or `BufferedFileReader`, but `FileReader` means something different in Java and JavaScript.
There are `CInFile` and `COutFile` here[https://android.googlesource.com/platform/external/lzma/+/kitkat-dev/CPP/Windows/FileIO.h <- google:‘"class CInFile"’],
so you can think of `IFile` and `OFile` as short forms of them.
*/
class IFile
{
protected:
    detail::FileHandle<true> fh;
    std::unique_ptr<uint8_t[]> buffer;
    size_t buffer_pos = 0, buffer_size = 0, buffer_capacity = IFILE_DEFAULT_BUFFER_SIZE;
    int64_t file_pos_of_buffer_start = 0;
    bool is_eof_reached = false;
    bool eof_indicator = false; // >[https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf <- https://en.wikipedia.org/wiki/C23_(C_standard_revision)]:‘The `feof` function tests the end-of-file indicator’

    void allocate_buffer()
    {
        if (buffer == nullptr)
            buffer.reset(new uint8_t[buffer_capacity]);
    }

    NOINLINE bool has_no_data_left()
    {
        assert(buffer_pos == buffer_size); // make sure there is no available data in the buffer

        if (is_eof_reached) { // check to prevent extra `read()` syscalls
            //assert(buffer_size != 0);
            file_pos_of_buffer_start += buffer_size;
            buffer_pos = buffer_size = 0;
            return true;
        }

        allocate_buffer();

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

    bool skip_bom(bool throw_ = true)
    {
        if (file_pos_of_buffer_start == 0 && buffer_pos == 0)
            if (buffer_size >= 3)
                if (is_bom(buffer.get())) {
                    if (buffer_size == 3) {
                        if (throw_)
                            throw UnexpectedEOF();
                        return true;
                    }
                    buffer_pos = 3;
                }
        return false;
    }

    static void handle_newlines(std::string &s)
    {
        // Replace all "\r\n" with "\n"
        size_t cr_pos = s.find('\r');
        if (cr_pos != std::string::npos) {
            char *dest = const_cast<char *>(s.c_str()) + cr_pos;
            const char *src = dest, *end = s.c_str() + s.size();
            while (src < end) {
                if (*src == '\r' && src[1] == '\n') {
                    src++;
                    continue;
                }
                *dest = *src;
                dest++;
                src++;
            }
            s.resize(dest - s.c_str());
        }
    }

public:
    template <class... Args> IFile(Args&&... args) : fh(std::forward<Args>(args)...) {}
    template <class... Args> bool open(Args&&... args) {return fh.open(std::forward<Args>(args)...);}
#if defined(_MSC_VER) && _MSC_VER <= 1800 // for `f = IFile(fname);` in MSVC 2013
    IFile(IFile &&f) : fh(std::move(f.fh)), buffer(std::move(f.buffer)), buffer_pos(f.buffer_pos), buffer_size(f.buffer_size), buffer_capacity(f.buffer_capacity), file_pos_of_buffer_start(f.file_pos_of_buffer_start), is_eof_reached(f.is_eof_reached), eof_indicator(f.eof_indicator) {}
    IFile &operator=(IFile &&f)
    {
        move_assign(this, std::move(f));
        return *this;
    }
#endif
//  ~IFile() {fh.close();}
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

    int64_t get_file_size()
    {
        int64_t file_size = fh.get_file_size();
        if (file_size == -2)
            throw FileSizeIsUnknown();
        return file_size;
    }

    int64_t tell() const
    {
        return file_pos_of_buffer_start + buffer_pos;
    }

    void seek(int64_t new_pos)
    {
        if (new_pos < 0)
            throw SeekFailed();

        is_eof_reached = eof_indicator = false;

        // First, verify that the new read position is within the buffer
        if (new_pos >= file_pos_of_buffer_start && new_pos <= file_pos_of_buffer_start + int64_t(buffer_size)) {
            buffer_pos = size_t(new_pos - file_pos_of_buffer_start);
            return;
        }

        // Roughly check that the file does not support positioning
        if (fh.get_file_size() == -2) {
            if (new_pos < tell())
                throw FileDoesNotSupportPositioning();

            // Emulate forward seek via reading
            do {
                buffer_pos = buffer_size;
            } while (!has_no_data_left() && new_pos > file_pos_of_buffer_start + int64_t(buffer_size));

            if (new_pos > file_pos_of_buffer_start + int64_t(buffer_size))
                throw SeekFailed();

            buffer_pos = size_t(new_pos - file_pos_of_buffer_start);
            return;
        }

        // Regular seek
        if (new_pos > get_file_size())
            throw SeekFailed();

        allocate_buffer();

        file_pos_of_buffer_start = new_pos & ~(IFILE_BUFFER_SIZE_RIGHT_AFTER_SEEK - 1);
        size_t how_much_to_read = IFILE_BUFFER_SIZE_RIGHT_AFTER_SEEK;
        if (file_pos_of_buffer_start + IFILE_BUFFER_SIZE_RIGHT_AFTER_SEEK - new_pos
                                     < IFILE_BUFFER_SIZE_RIGHT_AFTER_SEEK / 8) // if `new_pos` is too close to the end of the buffer, then read more
            how_much_to_read *= 2;
        buffer_size = fh.read(buffer.get(), how_much_to_read, file_pos_of_buffer_start);
        if (buffer_size < how_much_to_read)
            is_eof_reached = true;

        buffer_pos = size_t(new_pos - file_pos_of_buffer_start);
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
    `eof_passed()` function works like `eof()` in C++, i.e. it returns true if a read operation has attempted to read past the end of the file.
    The name of this function was inspired by:
    >[https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/feof]:‘the end of `stream` has been passed.’
    The name `eof_reached()` for this function has been rejected because it is confusing:
    >[-1]:‘The *‘eof’ function is used to determine when the end of the text file has been reached.’
    */
    bool eof_passed()
    {
        return eof_indicator;
    }

    uint8_t peek_byte()
    {
        if (at_eof())
            throw UnexpectedEOF();
        return buffer[buffer_pos];
    }

    uint8_t read_byte()
    {
        if (at_eof())
            throw UnexpectedEOF();
        return buffer[buffer_pos++];
    }

    std::string read_text() // reads whole file and returns its contents as a string; only works if the file pointer is at the beginning of the file (`read_text_to_end()` has no such limitation)
    {
        if (!(file_pos_of_buffer_start == 0 && buffer_pos == 0 && buffer_size == 0))
            throw ReadTextMustBeCalledAtTheBeginningOfTheFile();

        std::string file_str;
        int64_t file_size = fh.get_file_size();
        if (file_size != -2) {
            if (uint64_t(file_size) > SIZE_MAX)
                throw FileIsTooLargeToFitInMemory();
            size_t file_sz = (size_t)file_size;
            file_str.resize(file_sz);
            if (fh.read((char*)file_str.data(), file_sz) != file_sz)
                throw OSReportedIncorrectFileSize();
            file_pos_of_buffer_start = file_size;

            // Remove the BOM at the beginning of the file, if present
            if (file_str.length() >= 3 && is_bom((uint8_t*)file_str.data()))
                file_str.erase(0, 3);
        }
        else { // file size is unknown, so read via buffer
            if (!has_no_data_left()) {
                // Skip the BOM at the beginning of the file, if present
                if (buffer_size >= 3 && is_bom(buffer.get()))
                    buffer_pos = 3;

                // Read the rest
                do {
                    file_str.append(buffer.get() + buffer_pos, buffer.get() + buffer_size);
                    buffer_pos = buffer_size;
                } while (!has_no_data_left());
            }
        }

        handle_newlines(file_str);
        return file_str;
    }

    std::string read_text_to_end() // the method name was inspired by [https://doc.rust-lang.org/std/io/trait.Read.html#method.read_to_end]
    {
        if (at_eof())
            return std::string();

        // Skip the BOM at the beginning of the file, if present
        if (skip_bom(false))
            return std::string();

        // Read the rest
        std::string file_str(buffer.get() + buffer_pos, buffer.get() + buffer_size);
        buffer_pos = buffer_size;
        while (!has_no_data_left()) {
            file_str.append(buffer.get(), buffer.get() + buffer_size);
            buffer_pos = buffer_size;
        }

        handle_newlines(file_str);
        return file_str;
    }

    template <bool handle_nl = true> void read_until(std::string &res, char delim, bool keep_delim = false)
    {
        if (at_eof())
            throw UnexpectedEOF();

        // Skip the BOM at the beginning of the file, if present
        skip_bom();

        // Scan buffer for delim
        if (uint8_t *p = (uint8_t*)memchr(buffer.get() + buffer_pos, delim, buffer_size - buffer_pos)) {
            size_t res_size = p - (buffer.get() + buffer_pos);
            res.assign((char*)buffer.get() + buffer_pos, res_size + int(keep_delim));
            buffer_pos += res_size + 1;
            if (handle_nl) handle_newlines(res);
            return;
        }

        res.assign((char*)buffer.get() + buffer_pos, buffer_size - buffer_pos);
        buffer_pos = buffer_size;
        while (!has_no_data_left()) {
            if (uint8_t *p = (uint8_t*)memchr(buffer.get(), delim, buffer_size)) {
                res.append((char*)buffer.get(), p - buffer.get() + int(keep_delim));
                buffer_pos += p - buffer.get() + 1;
                if (handle_nl) handle_newlines(res);
                return;
            }
            res.append((char*)buffer.get(), buffer_size);
            buffer_pos = buffer_size;
        }
        if (handle_nl) handle_newlines(res);
    }

    template <bool handle_nl = true> std::string read_until(char delim, bool keep_delim = false)
    {
        std::string r;
        read_until<handle_nl>(r, delim, keep_delim);
        return r;
    }

    void read_line(std::string &r, bool keep_newline = false)
    {
        read_until<false>(r, '\n', keep_newline);
        if (!keep_newline) {
            if (!r.empty() && r.back() == '\r')
                r.pop_back();
        }
        else
            if (r.back() == '\n' && r.length() >= 2 && r[r.length() - 2] == '\r')
                r.erase(r.length() - 2, 1);
    }

    std::string read_line(bool keep_newline = false)
    {
        std::string r;
        read_line(r, keep_newline);
        return r;
    }

    std::string read_line_reae(bool keep_newline = false) // ‘r’‘e’‘a’‘e’ means ‘r’eturns ‘e’mpty [string] (‘a’t ‘e’nd of file);
    {                                                     // `read_line_reae()` corresponds to `std::getline()` in C++,
        if (at_eof()) {                                   // `read_line_reae(true)` corresponds to `readline()` in Python and to `read_line()` in Rust.
            eof_indicator = true;
            return std::string(); // when the EOF is reached, this function returns an empty string instead of throwing an UnexpectedEOF exception
        }

        // Skip the BOM at the beginning of the file, if present
        if (skip_bom(false)) {
            eof_indicator = true;
            return std::string(); // when the EOF is reached, this function returns an empty string instead of throwing an UnexpectedEOF exception
        }

        std::string r = read_until<false>('\n', true);
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

    uint32_t read_char()
    {
        if (at_eof())
            throw UnexpectedEOF();

        // Skip the BOM at the beginning of the file, if present
        skip_bom();

        uint8_t uchar[6];
        uchar[0] = buffer[buffer_pos++];
        uint8_t extraBytesToRead = utf::trailingBytesForUTF8[uchar[0]];
        read_bytes(uchar + 1, extraBytesToRead);

        bool ok = false;
        const char *source = (char*)uchar;
        char32_t ch = utf::decode(source, source + 1 + extraBytesToRead, ok);
        if (!ok)
            throw IFileUnicodeDecodeError();
        assert(source == (char*)uchar + 1 + extraBytesToRead);
        return ch;
    }

    std::vector<uint8_t> read_bytes()
    {
        if (!(file_pos_of_buffer_start == 0 && buffer_pos == 0 && buffer_size == 0))
            throw ReadBytesMustBeCalledAtTheBeginningOfTheFile();

        int64_t file_size = fh.get_file_size();
        if (file_size != -2) {
            if (uint64_t(file_size) > SIZE_MAX)
                throw FileIsTooLargeToFitInMemory();
            size_t file_sz = (size_t)file_size;
            std::vector<uint8_t> r(file_sz);
            if (fh.read(r.data(), file_sz) != file_sz)
                throw OSReportedIncorrectFileSize();
            file_pos_of_buffer_start = file_size;
            return r;
        }
        else { // file size is unknown, so read via buffer
            std::vector<uint8_t> r;
            while (!has_no_data_left()) {
                r.insert(r.end(), buffer.get(), buffer.get() + buffer_size);
                buffer_pos = buffer_size;
            }
            return r;
        }
    }

    std::vector<uint8_t> read_bytes_to_end()
    {
        int64_t file_size = fh.get_file_size();
        if (file_size != -2) {
            file_size -= tell();
            if (uint64_t(file_size) > SIZE_MAX)
                throw FileIsTooLargeToFitInMemory();
            //if (file_size == 0) // this `if` is needed to prevent the assertion `buffer_size != 0` in `has_no_data_left()`
            //    return std::vector<uint8_t>();
            return read_bytes((size_t)file_size);
        }
        else { // file size is unknown, so read via buffer
            std::vector<uint8_t> r(buffer.get() + buffer_pos, buffer.get() + buffer_size);
            buffer_pos = buffer_size;
            while (!has_no_data_left()) {
                r.insert(r.end(), buffer.get(), buffer.get() + buffer_size);
                buffer_pos = buffer_size;
            }
            return r;
        }
    }

    size_t read_bytes_at_most(uint8_t *p, size_t count)
    {
        if (at_eof())
            return 0;

        uint8_t *initial_p = p;

        while (true) {
            size_t n = (std::min)(buffer_size - buffer_pos, count);
            memcpy(p, buffer.get() + buffer_pos, n);
            p += n;
            buffer_pos += n;
            count -= n;
            if (count == 0)
                return p - initial_p;
            if (has_no_data_left()) {
                eof_indicator = true;
                return p - initial_p;
            }
        }
    }

    std::vector<uint8_t> read_bytes_at_most(size_t count)
    {
        std::vector<uint8_t> r(count);
        r.resize(read_bytes_at_most(r.data(), count));
        return r;
    }

    template <bool check_for_large_read = true> void read_bytes(uint8_t *p, size_t count)
    {
        if (at_eof())
            if (count != 0)
                throw UnexpectedEOF();

        if (check_for_large_read && count > buffer_capacity) { // optimize large reads (avoid extra `read()` syscalls)
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

    template <size_t count> void read_bytes(uint8_t *p)
    {
        read_bytes<(count > 512)>(p, count);
    }

    template <typename Struct> void read_struct(Struct &s)
    {
        read_bytes<sizeof(Struct)>((uint8_t*)&s);
    }

    std::vector<uint8_t> read_bytes(size_t count)
    {
        std::vector<uint8_t> r(count);
        read_bytes(r.data(), count);
        return r;
    }

    template <size_t count> std::array<uint8_t, count> read_bytes()
    {
        std::array<uint8_t, count> r;
        read_bytes<count>(r.data());
        return r;
    }

    bool starts_with(utf::std::string_view s) // s — signature/‘sequence of chars’
    {
        if (!(file_pos_of_buffer_start == 0 && buffer_pos == 0))
            throw StartsWithMustBeCalledAtTheBeginningOfTheFile();

        if (at_eof())
            return false;

        return buffer_size >= s.size() && memcmp(buffer.get(), s.data(), s.size()) == 0;
    }

    UnixNanotime   get_creation_time() {return   fh.get_creation_time();}
    UnixNanotime get_last_write_time() {return fh.get_last_write_time();}
};
