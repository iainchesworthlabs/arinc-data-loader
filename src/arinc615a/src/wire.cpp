#include "wire.hpp"

#include <algorithm>

#include "adl/arinc615a/limits.hpp"

namespace adl::arinc615a::detail {

// ---- Writer ----------------------------------------------------------------------------

std::byte* Writer::claim(std::size_t count) noexcept {
    const std::size_t start = size_;
    size_ += count;
    if (error_ || overflow_ || start + count > out_.size()) {
        overflow_ = overflow_ || !error_;
        return nullptr;
    }
    return out_.data() + start;
}

void Writer::u8(std::uint8_t value) noexcept {
    if (std::byte* p = claim(1)) {
        p[0] = static_cast<std::byte>(value);
    }
}

void Writer::u16(std::uint16_t value) noexcept {
    if (std::byte* p = claim(2)) {
        p[0] = static_cast<std::byte>(value >> 8);
        p[1] = static_cast<std::byte>(value & 0xFFU);
    }
}

void Writer::u32(std::uint32_t value) noexcept {
    if (std::byte* p = claim(4)) {
        p[0] = static_cast<std::byte>(value >> 24);
        p[1] = static_cast<std::byte>((value >> 16) & 0xFFU);
        p[2] = static_cast<std::byte>((value >> 8) & 0xFFU);
        p[3] = static_cast<std::byte>(value & 0xFFU);
    }
}

void Writer::bytes(std::span<const std::byte> data) noexcept {
    if (std::byte* p = claim(data.size())) {
        std::ranges::copy(data, p);
    }
}

void Writer::chars(std::string_view text) noexcept {
    if (std::byte* p = claim(text.size())) {
        std::ranges::transform(
            text, p, [](char c) { return static_cast<std::byte>(static_cast<unsigned char>(c)); });
    }
}

void Writer::c_string(std::string_view text, bool any_bytes) noexcept {
    for (const char c : text) {
        if (c == '\0') {
            fail(Error::malformed_text);
        } else if (!any_bytes && !is_text_byte(static_cast<unsigned char>(c))) {
            fail(Error::malformed_text);
        }
    }
    chars(text);
    u8(0);
}

void Writer::length_text(std::string_view text, bool required) noexcept {
    if (text.empty()) {
        if (required) {
            fail(Error::invalid_value);
        }
        u8(0);
        return;
    }
    if (text.size() > max_text_chars) {
        fail(Error::field_too_large);
        return;
    }
    if (!is_text(text)) {
        fail(Error::malformed_text);
        return;
    }
    u8(static_cast<std::uint8_t>(text.size() + 1));
    chars(text);
    u8(0);
}

void Writer::percent(std::uint8_t value) noexcept {
    if (value > 100) {
        fail(Error::invalid_value);
        return;
    }
    const char text[3] = {value >= 100 ? '1' : ' ',
                          value >= 10 ? static_cast<char>('0' + (value / 10) % 10) : ' ',
                          static_cast<char>('0' + value % 10)};
    chars(std::string_view{text, 3});
}

void Writer::count(std::size_t value) noexcept {
    if (value > 0xFFFFU) {
        fail(Error::field_too_large);
        return;
    }
    u16(static_cast<std::uint16_t>(value));
}

void Writer::patch_u32(std::size_t at, std::uint32_t value) noexcept {
    if (error_ || overflow_ || at + 4 > out_.size()) {
        return;
    }
    out_[at] = static_cast<std::byte>(value >> 24);
    out_[at + 1] = static_cast<std::byte>((value >> 16) & 0xFFU);
    out_[at + 2] = static_cast<std::byte>((value >> 8) & 0xFFU);
    out_[at + 3] = static_cast<std::byte>(value & 0xFFU);
}

Result<std::size_t> Writer::finish() const noexcept {
    if (error_) {
        return std::unexpected{*error_};
    }
    if (overflow_) {
        return std::unexpected{Error::buffer_too_small};
    }
    return size_;
}

Result<std::size_t> Writer::measure() const noexcept {
    if (error_) {
        return std::unexpected{*error_};
    }
    return size_;
}

// ---- Reader ----------------------------------------------------------------------------

std::uint8_t Reader::u8() noexcept {
    const auto data = bytes(1);
    return data.empty() ? std::uint8_t{0} : static_cast<std::uint8_t>(data[0]);
}

std::uint16_t Reader::u16() noexcept {
    const auto data = bytes(2);
    if (data.empty()) {
        return 0;
    }
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(data[0]) << 8) |
                                      std::to_integer<unsigned>(data[1]));
}

std::uint32_t Reader::u32() noexcept {
    const auto data = bytes(4);
    if (data.empty()) {
        return 0;
    }
    return (std::to_integer<std::uint32_t>(data[0]) << 24) |
           (std::to_integer<std::uint32_t>(data[1]) << 16) |
           (std::to_integer<std::uint32_t>(data[2]) << 8) | std::to_integer<std::uint32_t>(data[3]);
}

std::span<const std::byte> Reader::bytes(std::size_t count) noexcept {
    if (error_) {
        return {};
    }
    if (count > remaining()) {
        fail(Error::truncated);
        return {};
    }
    const auto data = data_.subspan(position_, count);
    position_ += count;
    return data;
}

std::string_view Reader::c_string(bool any_bytes) noexcept {
    if (error_) {
        return {};
    }
    const auto left = rest();
    const auto end = std::ranges::find(left, std::byte{0});
    if (end == left.end()) {
        fail(Error::truncated);
        return {};
    }
    const std::size_t length = static_cast<std::size_t>(end - left.begin());
    const std::string_view text{reinterpret_cast<const char*>(left.data()), length};
    if (!any_bytes && !is_text(text)) {
        fail(Error::malformed_text);
        return {};
    }
    position_ += length + 1;
    return text;
}

std::string_view Reader::length_text(bool required) noexcept {
    const std::size_t length = u8();
    const auto field = bytes(length);
    if (error_) {
        return {};
    }
    const auto end = std::ranges::find(field, std::byte{0});
    const std::string_view text{reinterpret_cast<const char*>(field.data()),
                                static_cast<std::size_t>(end - field.begin())};
    if (!is_text(text)) {
        fail(Error::malformed_text);
        return {};
    }
    if (required && text.empty()) {
        fail(Error::invalid_value);
        return {};
    }
    return text;
}

std::uint8_t Reader::percent() noexcept {
    const auto field = bytes(3);
    if (error_) {
        return 0;
    }
    std::size_t i = 0;
    while (i < 3 && field[i] == std::byte{' '}) {
        ++i;
    }
    if (i == 3) {
        fail(Error::invalid_value);
        return 0;
    }
    unsigned value = 0;
    for (; i < 3; ++i) {
        const unsigned digit = std::to_integer<unsigned>(field[i]) - static_cast<unsigned>('0');
        if (digit > 9) {
            fail(Error::invalid_value);
            return 0;
        }
        value = value * 10 + digit;
    }
    if (value > 100) {
        fail(Error::invalid_value);
        return 0;
    }
    return static_cast<std::uint8_t>(value);
}

void Reader::expect_end() noexcept {
    if (!error_ && remaining() != 0) {
        fail(Error::bad_length);
    }
}

}  // namespace adl::arinc615a::detail
