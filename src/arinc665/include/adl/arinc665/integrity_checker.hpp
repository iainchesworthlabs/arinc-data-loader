#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/crc.hpp"
#include "adl/arinc665/error.hpp"
#include "adl/arinc665/md5.hpp"
#include "adl/arinc665/sha1.hpp"

namespace adl::arinc665 {

// Computes the check value of one CheckValueType over the bytes it is fed. The
// algorithms are held by value in a variant, so choosing one by type at run
// time costs no allocation.
class IntegrityChecker {
public:
    // Fails with Error::unsupported_check_type for none, and for a type that
    // ARINC 665 has not defined.
    [[nodiscard]] static Result<IntegrityChecker> make(CheckValueType type) noexcept;

    [[nodiscard]] CheckValueType type() const noexcept { return type_; }

    // The Table 5.1 title of the type.
    [[nodiscard]] std::string_view title() const noexcept { return to_string(type_); }

    void reset() noexcept;
    void update(std::span<const std::byte> data) noexcept;

    // The check value of everything fed in since the last reset(), in the form
    // it is stored in a file. This allocates the value's bytes.
    [[nodiscard]] CheckValue check_value() const;

private:
    using Engine = std::variant<Crc8, Crc16, Crc32, Md5, Sha1>;

    IntegrityChecker(CheckValueType type, Engine engine) noexcept : type_(type), engine_(engine) {}

    CheckValueType type_;
    Engine engine_;
};

}  // namespace adl::arinc665
