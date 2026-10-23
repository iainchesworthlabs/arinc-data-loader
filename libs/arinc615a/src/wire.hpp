#pragma once

// How the bytes of the 615A formats are written and read, shared by every codec in the
// library and private to it. Everything is big endian. Neither class allocates, throws or
// reads or writes outside the span it was given.

#include "adl/arinc615a/error.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace adl::arinc615a::detail
{

// A byte is printable text if it is not a control character (0x00 to 0x1F, 0x7F). 8-bit
// values above 0x7F are let through: the standard says "8-bit ASCII" and nothing more.
[[nodiscard]] constexpr bool is_text_byte(unsigned char c) noexcept
{
    return c >= 0x20U && c != 0x7FU;
}

[[nodiscard]] constexpr bool is_text(std::string_view text) noexcept
{
    for (const char c : text)
    {
        if (!is_text_byte(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }
    return true;
}

// Writes into a span, and keeps counting when the span is full, so that one pass over a
// value also measures it. The first thing that goes wrong is remembered (like a stream's
// error flag); after that nothing more is written.
class Writer
{
public:
    explicit Writer(std::span<std::byte> out) noexcept : out_(out) {}

    void u8(std::uint8_t value) noexcept;
    void u16(std::uint16_t value) noexcept;
    void u32(std::uint32_t value) noexcept;
    void bytes(std::span<const std::byte> data) noexcept;
    void chars(std::string_view text) noexcept;

    // Text with no length, ended by a NUL (TFTP, FIND). It may not hold a NUL. A control
    // character is Error::malformed_text unless `any_bytes`.
    void c_string(std::string_view text, bool any_bytes = false) noexcept;

    // The text fields of the protocol files: an 8-bit length, then the characters and a
    // NUL, counted in the length. An optional empty text is a length of 0 and nothing
    // else; a required one is Error::invalid_value.
    void length_text(std::string_view text, bool required) noexcept;

    // Three characters, right adjusted, blank filled: 0 to 100.
    void percent(std::uint8_t value) noexcept;

    // A 16-bit count of entries.
    void count(std::size_t value) noexcept;

    void patch_u32(std::size_t at, std::uint32_t value) noexcept;

    void fail(Error error) noexcept
    {
        if (!error_)
        {
            error_ = error;
        }
    }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return size_;
    }

    // The bytes written, or the first error, or buffer_too_small.
    [[nodiscard]] Result<std::size_t> finish() const noexcept;
    // The bytes the whole would take, or the first error other than buffer_too_small.
    [[nodiscard]] Result<std::size_t> measure() const noexcept;

private:
    std::byte* claim(std::size_t count) noexcept;

    std::span<std::byte> out_;
    std::size_t size_ = 0;
    bool overflow_ = false;
    std::optional<Error> error_;
};

// Reads from a span. After the first failure every read returns zero or nothing, so a
// decoder can read a whole structure and look once at the end.
class Reader
{
public:
    explicit Reader(std::span<const std::byte> data) noexcept : data_(data) {}

    [[nodiscard]] bool ok() const noexcept
    {
        return !error_.has_value();
    }
    [[nodiscard]] Error error() const noexcept
    {
        return error_.value_or(Error::truncated);
    }
    [[nodiscard]] Result<void> status() const noexcept
    {
        return error_ ? Result<void>{std::unexpected{*error_}} : Result<void>{};
    }
    void fail(Error error) noexcept
    {
        if (!error_)
        {
            error_ = error;
        }
    }

    [[nodiscard]] std::size_t position() const noexcept
    {
        return position_;
    }
    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return data_.size() - position_;
    }
    [[nodiscard]] std::span<const std::byte> rest() const noexcept
    {
        return data_.subspan(position_);
    }

    [[nodiscard]] std::uint8_t u8() noexcept;
    [[nodiscard]] std::uint16_t u16() noexcept;
    [[nodiscard]] std::uint32_t u32() noexcept;
    [[nodiscard]] std::span<const std::byte> bytes(std::size_t count) noexcept;

    // A NUL-terminated text. Error::truncated if no NUL comes; Error::malformed_text for
    // a control character, unless `any_bytes`.
    [[nodiscard]] std::string_view c_string(bool any_bytes = false) noexcept;

    // See Writer::length_text. The characters are the field up to its first NUL (or all of
    // it, if it has none); what follows a NUL inside the field is padding and is dropped.
    [[nodiscard]] std::string_view length_text(bool required) noexcept;

    [[nodiscard]] std::uint8_t percent() noexcept;

    // Error::bad_length unless the data has all been read.
    void expect_end() noexcept;

private:
    std::span<const std::byte> data_;
    std::size_t position_ = 0;
    std::optional<Error> error_;
};

} // namespace adl::arinc615a::detail
