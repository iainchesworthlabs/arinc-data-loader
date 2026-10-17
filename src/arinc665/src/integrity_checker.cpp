#include "adl/arinc665/integrity_checker.hpp"

#include <algorithm>
#include <array>
#include <type_traits>

namespace adl::arinc665 {

namespace {

// A CRC as a stored check value: most significant byte first, and padded with
// leading zeros to a whole number of 16-bit words (section 5.1).
template <typename Crc>
std::vector<std::byte> crc_bytes(const Crc& crc) {
    constexpr std::size_t width = sizeof(typename Crc::value_type);
    constexpr std::size_t stored = width < 2 ? 2 : width;

    std::vector<std::byte> bytes(stored, std::byte{0});
    const auto value = crc.value();
    for (std::size_t i = 0; i < width; ++i) {
        bytes[stored - 1 - i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
    }
    return bytes;
}

template <typename Digest>
std::vector<std::byte> digest_bytes(const Digest& digest) {
    return std::vector<std::byte>(digest.begin(), digest.end());
}

}  // namespace

Result<IntegrityChecker> IntegrityChecker::make(CheckValueType type) noexcept {
    switch (type) {
        case CheckValueType::crc8:
            return IntegrityChecker{type, Crc8{}};
        case CheckValueType::crc16:
            return IntegrityChecker{type, Crc16{}};
        case CheckValueType::crc32:
            return IntegrityChecker{type, Crc32{}};
        case CheckValueType::md5:
            return IntegrityChecker{type, Md5{}};
        case CheckValueType::sha1:
            return IntegrityChecker{type, Sha1{}};
        case CheckValueType::none:
            break;
    }
    return std::unexpected{Error::unsupported_check_type};
}

void IntegrityChecker::reset() noexcept {
    std::visit([](auto& engine) { engine.reset(); }, engine_);
}

void IntegrityChecker::update(std::span<const std::byte> data) noexcept {
    std::visit([data](auto& engine) { engine.update(data); }, engine_);
}

CheckValue IntegrityChecker::check_value() const {
    CheckValue result;
    result.type = type_;
    std::visit(
        [&result](const auto& engine) {
            using Algorithm = std::decay_t<decltype(engine)>;
            if constexpr (std::is_same_v<Algorithm, Md5> || std::is_same_v<Algorithm, Sha1>) {
                result.bytes = digest_bytes(engine.digest());
            } else {
                result.bytes = crc_bytes(engine);
            }
        },
        engine_);
    return result;
}

Result<CheckValue> compute_check_value(CheckValueType type, std::span<const std::byte> data) {
    auto checker = IntegrityChecker::make(type);
    if (!checker) {
        return std::unexpected{checker.error()};
    }
    checker->update(data);
    return checker->check_value();
}

}  // namespace adl::arinc665
