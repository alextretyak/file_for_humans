const size_t IFILE_DEFAULT_BUFFER_SIZE = 32*1024;

class IFile
{
#ifdef _WIN32
HANDLE handle;
#endif

public:
    /*
    `at_eof()` function works like `eof()` in Pascal, i.e. it returns true if the file is at the end.
    The name `at_the_end()` for this function has been rejected as it is longer, and the name `at_eof()` is still quite correct:
    >[http://www.irietools.com/iriepascal/progref177.html <- google:‘pascal eof’]:‘file is at end-of-file’
    */
    bool at_eof()
    {
    }

    /*
    `eof_reached()` function works like `eof()` in C, i.e. it returns true if a read operation has attempted to read past the end of the file.
    The name of this function was inspired by:
    >[https://en.cppreference.com/w/cpp/io/basic_ios/eof]:‘reached end-of-file’
    */
    bool eof_reached()
    {
    }
};
