#pragma once

// Allocation that reports failure instead of throwing, for the platform
// implementations. Their interfaces are noexcept and say `resource_exhausted`
// when memory runs out; a plain `new` would call std::terminate instead.

#include <memory>
#include <new>
#include <utility>

namespace adl::platform::detail
{

// nullptr if the allocation fails. T's constructor must not throw.
template <class T, class... Args>
[[nodiscard]] std::unique_ptr<T> make_unique_nothrow(Args&&... args) noexcept
{
    return std::unique_ptr<T>(new (std::nothrow) T(std::forward<Args>(args)...));
}

} // namespace adl::platform::detail
