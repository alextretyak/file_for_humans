#pragma once

class UnixNanotime
{
    UnixNanotime() = delete;
    UnixNanotime(uint64_t n) : nanoseconds_since_epoch(n) {}
    uint64_t nanoseconds_since_epoch;

    static const uint64_t BOUNDARY = 0xE000000000000000; // 0b11100000'00000000'00000000'00000000'00000000'00000000'00000000'00000000

public:
    static UnixNanotime from_time_t(time_t t) {return UnixNanotime(t * 1000000000);}
    static UnixNanotime from_nanotime_t(uint64_t nt) {return UnixNanotime(nt);}
    static UnixNanotime uninitialized() {return UnixNanotime(BOUNDARY);}

    time_t to_time_t() const
    {
        if (nanoseconds_since_epoch < BOUNDARY)
            return nanoseconds_since_epoch / 1000000000u;
        else
            return int64_t(nanoseconds_since_epoch) / 1000000000;
    }

    bool operator==(const UnixNanotime nt) const {return nanoseconds_since_epoch == nt.nanoseconds_since_epoch;}
};
