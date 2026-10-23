#include "adl/arinc615a/checksum.hpp"

#include <algorithm>

namespace adl::arinc615a {

bool can_calculate(const ChecksumOption& option) noexcept {
    return adl::arinc665::IntegrityChecker::make(option.type).has_value();
}

void ChecksumVerifier::begin(const std::optional<ChecksumOption>& option) noexcept {
    checker_.reset();
    state_ = ChecksumVerdict::not_given;
    if (!option) {
        return;
    }
    auto checker = adl::arinc665::IntegrityChecker::make(option->type);
    if (!checker) {
        state_ = ChecksumVerdict::unsupported;
        return;
    }
    checker_ = *checker;
    expected_ = *option;
    state_ = ChecksumVerdict::matched;  // until finish() compares
}

ChecksumVerdict ChecksumVerifier::finish() const {
    if (!checker_) {
        return state_;
    }
    const adl::arinc665::CheckValue value = checker_->check_value();
    const auto expected = expected_.bytes();
    return std::ranges::equal(value.bytes, expected) ? ChecksumVerdict::matched
                                                     : ChecksumVerdict::mismatched;
}

}  // namespace adl::arinc615a
