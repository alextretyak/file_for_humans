#include <vector>
#include <cstdint> // for uint8_t
#include "FileHandle.hpp"
#include <memory> // for std::unique_ptr
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
    bool is_eof_reached = false;

    NOINLINE bool has_no_data_left()
    {
        assert(buffer_pos == buffer_size); // make sure there is no available data in the buffer

        if (is_eof_reached) // check to prevent extra `read()` syscalls
            return true;

        if (buffer == nullptr)
            buffer.reset(new uint8_t[buffer_capacity]);

        buffer_size = fh.read(buffer.get(), buffer_capacity);
        if (buffer_size < buffer_capacity)
            is_eof_reached = true;

        buffer_pos = 0;

        return buffer_size == 0;
    }

public:
    template <class... Args> IFile(Args&&... args) : fh(std::forward<Args>(args)...) {}
    template <class... Args> bool open(Args&&... args) {return fh.open(std::forward<Args>(args)...);}
    ~IFile() {close();}
    void close() {fh.close();}

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

    std::string read_line()
    {
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
};
