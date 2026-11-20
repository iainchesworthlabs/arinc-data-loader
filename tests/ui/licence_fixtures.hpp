// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Licences for the tests of the licence model and of the windows that show it: a Pro licence for 13 November
// 2026 to 13 November 2027, signed with the published test key of RFC 8032 (the only key a test has: no real key
// exists in this repository), and the times a test judges it at.

#include "adl/licence/licence.hpp"
#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/test_keys.hpp"

#include <QtGlobal>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace adl::ui_test
{

namespace lic = adl::licence;

inline lic::TimePoint at(int year, unsigned month, unsigned day, int hour = 0)
{
    return lic::TimePoint{
               std::chrono::sys_days{std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}}} +
           std::chrono::hours{hour};
}

inline lic::Licence pro()
{
    lic::Licence value;
    value.product = "adl";
    value.licence_id = "ADL-2026-000123";
    value.kind = lic::LicenceKind::annual;
    value.edition = lic::Edition::pro;
    value.licensee_name = "Example Lab Pty Ltd";
    value.issued_at = at(2026, 11, 13, 2);
    value.starts_at = at(2026, 11, 13);
    value.expires_at = at(2027, 11, 13);
    value.grace_days = 28;
    value.seats = 3;
    value.features = {"support"};
    return value;
}

// The text of a licence file for `value`, signed by the key `key_id`.
inline std::string signed_file_of(const lic::Licence& value, std::string_view key_id = lic::test_keys::key_id)
{
    const auto payload = lic::write_payload(value);
    if (!payload)
    {
        qFatal("the test licence is not valid");
    }
    const std::span<const std::uint8_t> bytes{reinterpret_cast<const std::uint8_t*>(payload->data()), payload->size()};
    return lic::signed_file::write(
        lic::signing::sign(lic::licence_purpose, key_id, bytes, lic::test_keys::private_key()));
}

} // namespace adl::ui_test
