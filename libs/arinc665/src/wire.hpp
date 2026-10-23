#pragma once

// How ARINC 665 files are laid out on the wire, shared by every file in the
// library and private to it. Everything is counted in 16-bit words, and every
// multi-byte value is most significant byte first (section 3.4).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/error.hpp"

namespace adl::arinc665::detail {

// The widest a count, a string length or a check value length can be: they are
// 16-bit fields.
inline constexpr std::size_t max_field_value = 0xFFFFU;

// Writes fields to a file. Like a stream, it remembers the first thing that went
// wrong (a value that does not fit its field): after that ok() is false, and the
// caller checks status() once at the end.
class WordWriter {
public:
    [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
    [[nodiscard]] Result<void> status() const noexcept {
        return error_ ? Result<void>{std::unexpected{*error_}} : Result<void>{};
    }
    void fail(Error error) noexcept {
        if (!error_) {
            error_ = error;
        }
    }

    [[nodiscard]] std::size_t words() const noexcept { return bytes_.size() / 2; }
    [[nodiscard]] std::span<const std::byte> view() const noexcept { return bytes_; }
    [[nodiscard]] std::vector<std::byte> take() noexcept { return std::move(bytes_); }

    void u16(std::uint16_t value);
    void u32(std::uint32_t value);
    void u64(std::uint64_t value);

    // Two 8-bit values sharing a word, the first in the most significant byte
    // (the media sequence number X and the number of members Y).
    void u8_pair(std::uint8_t first, std::uint8_t second);

    // Raw bytes; the caller keeps the output a whole number of words.
    void bytes(std::span<const std::byte> data);

    // The form of every text field in the standard: the length in characters as
    // a 16-bit word, the characters, and a NUL if their number is odd.
    void string(std::string_view text);

    // A count that must fit a 16-bit field.
    void count(std::size_t value);

    // The three fields every check value takes (length, type, value), or the one
    // 0x0000 word when there is none (sections 2.2.3.1.40 to 2.2.3.1.42).
    void check_value(const OptionalCheckValue& value);

    // A value that must fit a 16-bit or 32-bit field.
    [[nodiscard]] std::uint16_t fit_u16(std::size_t value) noexcept;
    [[nodiscard]] std::uint32_t fit_u32(std::size_t value) noexcept;

    void patch_u16(std::size_t word, std::uint16_t value) noexcept;
    void patch_u32(std::size_t word, std::uint32_t value) noexcept;

private:
    std::vector<std::byte> bytes_;
    std::optional<Error> error_;
};

// Reads fields from a file. Like a stream, it remembers the first thing that went
// wrong: after that every read returns zero or empty, and ok() is false, so a
// decoder can read a whole section and check once at the end.
class WordReader {
public:
    explicit WordReader(std::span<const std::byte> data) noexcept : data_(data) {}

    [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
    [[nodiscard]] Error error() const noexcept { return error_.value_or(Error::truncated); }
    [[nodiscard]] Result<void> status() const noexcept {
        return error_ ? Result<void>{std::unexpected{*error_}} : Result<void>{};
    }

    [[nodiscard]] std::size_t size_words() const noexcept { return data_.size() / 2; }
    [[nodiscard]] std::size_t position() const noexcept { return position_ / 2; }

    // Moves to `word`; the end of the data is a valid place to be. Anywhere
    // past it is Error::bad_pointer.
    void seek(std::size_t word) noexcept;

    [[nodiscard]] std::uint16_t u16() noexcept;
    [[nodiscard]] std::uint32_t u32() noexcept;
    [[nodiscard]] std::uint64_t u64() noexcept;
    [[nodiscard]] std::pair<std::uint8_t, std::uint8_t> u8_pair() noexcept;
    [[nodiscard]] std::span<const std::byte> bytes(std::size_t count) noexcept;
    [[nodiscard]] std::string string();

    // The check value fields; absent when the length is 0x0000.
    [[nodiscard]] OptionalCheckValue check_value();

    // Records `error` as the first failure, if there is not one already.
    void fail(Error error) noexcept {
        if (!error_) {
            error_ = error;
        }
    }

private:
    std::span<const std::byte> data_;
    std::size_t position_ = 0;  // bytes
    std::optional<Error> error_;
};

// The CRCs of a whole file's bytes, for the file-level fields.
[[nodiscard]] std::uint16_t crc16(std::span<const std::byte> data) noexcept;
[[nodiscard]] std::uint32_t crc32(std::span<const std::byte> data) noexcept;

}  // namespace adl::arinc665::detail
