#pragma once

// A sequence of T that is either a caller's array or the entries of a decoded file,
// read from the bytes as they are walked. Either way nothing is copied or allocated,
// so the same type serves the encoders (given a std::array or a std::vector) and the
// decoders (which hand back the bytes they were given, already checked).
//
// A List does not own what it views: the array, or the bytes of the decoded file, must
// outlive it, and so must the strings that the entries themselves view.

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>

namespace adl::arinc615a {

template <typename T>
class List {
public:
    // Reads the next entry from the front of `rest` and moves `rest` past it. Given
    // only bytes the decoder has checked, so it cannot fail.
    using ReadFn = T (*)(std::span<const std::byte>& rest) noexcept;

    constexpr List() noexcept = default;
    constexpr List(std::span<const T> items) noexcept : items_(items), count_(items.size()) {}

    template <typename R>
        requires(std::ranges::contiguous_range<const R&> && std::ranges::sized_range<const R&> &&
                 std::same_as<std::ranges::range_value_t<const R&>, T> &&
                 !std::same_as<std::remove_cvref_t<R>, List>)
    constexpr List(const R& range) noexcept : List(std::span<const T>{range}) {}

    // For the decoders: `count` entries laid end to end in `bytes`.
    [[nodiscard]] static constexpr List from_wire(std::span<const std::byte> bytes,
                                                  std::size_t count, ReadFn read) noexcept {
        List list;
        list.bytes_ = bytes;
        list.read_ = read;
        list.count_ = count;
        return list;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return count_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return count_ == 0; }

    class Iterator {
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = T;  // an entry is read from the bytes each time, not stored
        using pointer = void;
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;

        constexpr Iterator() noexcept = default;

        [[nodiscard]] constexpr T operator*() const noexcept {
            if (read_ == nullptr) {
                return *item_;
            }
            auto copy = rest_;
            return read_(copy);
        }
        constexpr Iterator& operator++() noexcept {
            if (read_ == nullptr) {
                ++item_;
            } else {
                static_cast<void>(read_(rest_));
            }
            --left_;
            return *this;
        }
        constexpr void operator++(int) noexcept { ++*this; }

        [[nodiscard]] friend constexpr bool operator==(const Iterator& a,
                                                       const Iterator& b) noexcept {
            return a.left_ == b.left_;
        }

    private:
        friend class List;
        const T* item_ = nullptr;
        std::span<const std::byte> rest_;
        ReadFn read_ = nullptr;
        std::size_t left_ = 0;
    };

    [[nodiscard]] constexpr Iterator begin() const noexcept {
        Iterator it;
        it.item_ = items_.data();
        it.rest_ = bytes_;
        it.read_ = read_;
        it.left_ = count_;
        return it;
    }
    [[nodiscard]] constexpr Iterator end() const noexcept { return Iterator{}; }

    // The entries compare equal one by one, wherever they are held.
    [[nodiscard]] friend constexpr bool operator==(const List& a, const List& b) noexcept {
        if (a.count_ != b.count_) {
            return false;
        }
        auto x = a.begin();
        auto y = b.begin();
        for (; x != a.end(); ++x, ++y) {
            if (!(*x == *y)) {
                return false;
            }
        }
        return true;
    }

private:
    std::span<const T> items_;
    std::span<const std::byte> bytes_;
    ReadFn read_ = nullptr;
    std::size_t count_ = 0;
};

}  // namespace adl::arinc615a
