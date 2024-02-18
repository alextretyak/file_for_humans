#pragma once


template <class Handle, Handle default_value> class UniqueHandle
{
    Handle handle;

    UniqueHandle(const UniqueHandle &) = delete;
    void operator=(const UniqueHandle &) = delete;

public:
    UniqueHandle() : handle(default_value) {}
    UniqueHandle(UniqueHandle &&other) : handle(other.handle) {other.handle = default_value;}

    operator Handle() const {return handle;}
    Handle operator=(Handle h) {return handle = h;}
//  operator=(UniqueHandle &&) is not defined here because UniqueHandle does not know how to close handle correctly, and it is better to use `move_assign()` anyway
};


template <class Ty> void move_assign(Ty *dest, Ty &&other)
{
    if (dest != &other) { // >[https://learn.microsoft.com/en-us/cpp/cpp/move-constructors-and-move-assignment-operators-cpp?view=msvc-170 <- google:‘move constructor’]:‘if (this != &other)’
        dest->~Ty();
        new(dest)Ty(std::move(other));
    }
}
