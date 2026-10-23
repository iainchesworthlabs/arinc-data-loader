#pragma once

// Checking a received file against the checksum option the target sent with its write request
// (section 5.3.2.3.8.5). The algorithm is ARINC 665's (the option is named for its Table 5.1
// number); the value is calculated as the bytes arrive, so the file is never held to be summed.
// The standard is firm that the result is for the operator only: a file that does not match is
// handled exactly like one that does, and the transfer does not fail. So the verifier only
// reports; it never decides anything.

#include <cstddef>
#include <optional>
#include <span>

#include "adl/arinc615a/tftp.hpp"
#include "adl/arinc665/integrity_checker.hpp"

namespace adl::arinc615a {

enum class ChecksumVerdict : unsigned char {
    not_given,    // the target sent no checksum option
    unsupported,  // the algorithm cannot be calculated here (the option was not acknowledged)
    matched,
    mismatched,
};

[[nodiscard]] constexpr const char* to_string(ChecksumVerdict verdict) noexcept {
    switch (verdict) {
        case ChecksumVerdict::not_given:
            return "no checksum given";
        case ChecksumVerdict::unsupported:
            return "checksum algorithm not supported";
        case ChecksumVerdict::matched:
            return "checksum matched";
        case ChecksumVerdict::mismatched:
            return "checksum did not match";
    }
    return "unknown";
}

// Whether the option names an algorithm this library can calculate. The option is acknowledged
// only if it does (RFC 2347: an option the receiver does not take is left out of the OACK).
[[nodiscard]] bool can_calculate(const ChecksumOption& option) noexcept;

class ChecksumVerifier {
public:
    // Starts checking against `option`, or does nothing if there is none or it cannot be
    // calculated (finish() then says so).
    void begin(const std::optional<ChecksumOption>& option) noexcept;

    void update(std::span<const std::byte> data) noexcept {
        if (checker_) {
            checker_->update(data);
        }
    }

    // The verdict over everything fed in. Calculating the value allocates its few bytes (the
    // ARINC 665 library returns it in a vector): one small allocation per file that has a
    // checksum, and none for a file that has not.
    [[nodiscard]] ChecksumVerdict finish() const;

private:
    std::optional<adl::arinc665::IntegrityChecker> checker_;
    ChecksumOption expected_{};
    ChecksumVerdict state_ = ChecksumVerdict::not_given;
};

}  // namespace adl::arinc615a
