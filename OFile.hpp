#pragma once
#include <cstdint> // for uint8_t
#include "FileHandle.hpp"
#include <memory> // for std::unique_ptr

class OFile
{
    detail::FileHandle<false> fh;

public:
    template <class... Args> OFile(Args&&... args) : fh(std::forward<Args>(args)...) {}
    template <class... Args> bool open(Args&&... args) {return fh.open(std::forward<Args>(args)...);}
    void close()
    {
        fh.close();
    }

    void set_last_write_time(UnixNanotime t)
    {
        fh.set_last_write_time(t);
    }
};
