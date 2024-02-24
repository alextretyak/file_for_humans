#pragma once
#include <cstdint> // for uint8_t
#include "FileHandle.hpp"
#include <memory> // for std::unique_ptr
#include <vector>

const size_t OFILE_DEFAULT_BUFFER_SIZE = 32*1024;

class OFileBufferAlreadyAllocated {};

class OFile
{
    detail::FileHandle<false> fh;
    std::unique_ptr<uint8_t[]> buffer;
    size_t buffer_pos = 0, buffer_capacity = OFILE_DEFAULT_BUFFER_SIZE;

public:
    template <class... Args> OFile(Args&&... args) : fh(std::forward<Args>(args)...) {}
    template <class... Args> bool open(Args&&... args) {return fh.open(std::forward<Args>(args)...);}
    ~OFile() {flush();}
    void close()
    {
        flush();
        fh.close();
        buffer_pos = 0;
    }

    void set_buffer_size(size_t sz)
    {
        if (buffer != nullptr)
            throw OFileBufferAlreadyAllocated();
        buffer_capacity = sz;
    }

    void flush()
    {
        if (buffer_pos != 0) {
            fh.write(buffer.get(), buffer_pos);
            buffer_pos = 0;
        }
    }

    void write(const void *vp, size_t sz)
    {
        if (buffer == nullptr)
            buffer.reset(new uint8_t[buffer_capacity]);

        if (sz > buffer_capacity) { // optimize large writes (avoid extra `write()` syscalls)
            flush(); // first of all, write all of the remaining bytes in the buffer
            fh.write(vp, sz);
            return;
        }

        uint8_t *p = (uint8_t*)vp;

        while (true) {
            size_t n = (std::min)(buffer_capacity - buffer_pos, sz);
            memcpy(buffer.get() + buffer_pos, p, n);
            buffer_pos += n;
            if (buffer_pos == buffer_capacity) {
                fh.write(buffer.get(), buffer_capacity);
                buffer_pos = 0;
            }
            sz -= n;
            if (sz == 0)
                return;
            p += n;
        }
    }

    void write(const std::vector<uint8_t> &v)
    {
        write(v.data(), v.size());
    }

    void write(utf::std::string_view sv)
    {
        write(sv.data(), sv.size());
    }

    void set_last_write_time(UnixNanotime t)
    {
        fh.set_last_write_time(t);
    }
};
