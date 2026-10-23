#pragma once

// The handle the platform interfaces return for an object it made: a socket, an
// open file, a thread, a lock, a queue. It is a std::unique_ptr whose deleter
// can hand the object back to the service that made it, which is what lets a
// platform with no heap keep its objects in fixed slots and still return them by
// value. On a desktop the deleter is empty and the handle is a plain
// owning pointer: the object is deleted.
//
//     Result<Owned<UdpSocket>> socket = network.open_udp(options);
//     (*socket)->send_to(...);            // used as a pointer
//                                        // released when it goes out of scope
//
// A service that recycles its objects implements Recycler<T> for each type it
// hands out and returns handles whose deleter points at it. The object must be
// destroyed in place (its destructor is virtual) before its slot is reused.

#include <memory>
#include <new>
#include <utility>

namespace adl::platform
{

template <class T>
class Recycler
{
public:
    // Destroys `object` and frees whatever held it. Called by the handle.
    virtual void recycle(T* object) noexcept = 0;

protected:
    ~Recycler() = default;
};

template <class T>
struct Delete
{
    Recycler<T>* recycler = nullptr; // null: delete it

    void operator()(T* object) const noexcept
    {
        if (object == nullptr)
        {
            return;
        }
        if (recycler != nullptr)
        {
            recycler->recycle(object);
        }
        else
        {
            delete object;
        }
    }
};

template <class T>
using Owned = std::unique_ptr<T, Delete<T>>;

// Makes a `Derived` with a plain, non-throwing new and returns it as an owning
// handle to `Base`; empty if the allocation failed. `Derived`'s constructor
// must not throw. For the platform implementations that run on a heap.
template <class Base, class Derived, class... Args>
[[nodiscard]] Owned<Base> make_owned_nothrow(Args&&... args) noexcept
{
    Derived* object = new (std::nothrow) Derived(std::forward<Args>(args)...);
    return Owned<Base>(object, Delete<Base>{});
}

// Takes a `Derived` that a caller already made with new, as a handle to `Base`.
template <class Base, class Derived>
[[nodiscard]] Owned<Base> adopt(std::unique_ptr<Derived> object) noexcept
{
    return Owned<Base>(object.release(), Delete<Base>{});
}

} // namespace adl::platform
