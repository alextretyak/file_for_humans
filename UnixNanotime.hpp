#pragma once

class UnixNanotime
{
    UnixNanotime() = delete;
    UnixNanotime(uint64_t n) : nanoseconds_since_epoch(n) {}
    uint64_t nanoseconds_since_epoch;

    static const uint64_t BOUNDARY = 0xE000000000000000; // 0b11100000'00000000'00000000'00000000'00000000'00000000'00000000'00000000

public:
    static UnixNanotime from_time_t(time_t t) {return UnixNanotime(t * 1000000000);}
    static UnixNanotime from_nanotime_t(uint64_t nt) {return UnixNanotime(nt != BOUNDARY ? nt : BOUNDARY + 1);}
    static UnixNanotime uninitialized() {return UnixNanotime(BOUNDARY);}

    time_t to_time_t() const
    {
        if (nanoseconds_since_epoch < BOUNDARY)
            return nanoseconds_since_epoch / 1000000000u;
        else
            return int64_t(nanoseconds_since_epoch) / 1000000000;
    }

    template <unsigned scale, int64_t offset> uint64_t to_uint64() const
    {
        if (nanoseconds_since_epoch < BOUNDARY)
            return uint64_t(nanoseconds_since_epoch / scale + offset);
        else
            return uint64_t(int64_t(nanoseconds_since_epoch) / int(scale) + offset);
    }

    template <class TimeSpec> void to_timespec(TimeSpec &ts) const
    {
        if (nanoseconds_since_epoch < BOUNDARY) {
            ts.tv_sec  = nanoseconds_since_epoch / 1000000000u;
            ts.tv_nsec = nanoseconds_since_epoch % 1000000000u;
        }
        else {
            ts.tv_sec  =   int64_t(nanoseconds_since_epoch) / 1000000000;
            ts.tv_nsec = -(int64_t(nanoseconds_since_epoch) % 1000000000);
        }
    }

    bool operator==(const UnixNanotime nt) const {return nanoseconds_since_epoch == nt.nanoseconds_since_epoch;}
};
