// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the licence tests share: a licence to judge, the way to sign it with the published test key, and
// the context to judge it in.

#include "adl/licence/licence.hpp"
#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/test_keys.hpp"
#include "bytes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::test
{

[[nodiscard]] inline licence::TimePoint at(int year, unsigned month, unsigned day, int hour = 0)
{
    return licence::TimePoint{
               std::chrono::sys_days{std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}}} +
           std::chrono::hours{hour};
}

[[nodiscard]] inline std::span<const std::uint8_t> span_of(std::string_view text)
{
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

// Hexadecimal as bytes, and as the text those bytes spell.
[[nodiscard]] inline std::vector<std::uint8_t> hex_bytes(std::string_view hex)
{
    std::vector<std::uint8_t> bytes;
    for (const std::byte byte : from_hex(hex))
    {
        bytes.push_back(std::to_integer<std::uint8_t>(byte));
    }
    return bytes;
}

[[nodiscard]] inline std::string hex_text(std::string_view hex)
{
    const auto bytes = hex_bytes(hex);
    return {bytes.begin(), bytes.end()};
}

// An annual Pro licence for 13 November 2026 to 13 November 2027, as tests/golden/make_golden_licence.py
// writes it.
[[nodiscard]] inline licence::Licence annual()
{
    licence::Licence value;
    value.product = "adl";
    value.licence_id = "ADL-2026-000123";
    value.kind = licence::LicenceKind::annual;
    value.edition = licence::Edition::pro;
    value.licensee_name = "Example Lab Pty Ltd";
    value.licensee_contact = "Test team";
    value.issued_at = at(2026, 11, 13, 2);
    value.starts_at = at(2026, 11, 13);
    value.expires_at = at(2027, 11, 13);
    value.grace_days = 28;
    value.seats = 3;
    value.features = {"support"};
    return value;
}

// An Enterprise licence with every optional field, as the same script writes it.
[[nodiscard]] inline licence::Licence full()
{
    licence::Licence value;
    value.product = "adl";
    value.licence_id = "ADL-2026-000124";
    value.kind = licence::LicenceKind::enterprise;
    value.edition = licence::Edition::enterprise;
    value.licensee_name = "Example Avionics Ltd";
    value.licensee_contact = "Certification team";
    value.licensee_email = "tests@example-lab.invalid";
    value.issued_at = at(2026, 11, 13, 2) + std::chrono::minutes{10} + std::chrono::seconds{5};
    value.starts_at = at(2026, 11, 13);
    value.expires_at = at(2028, 11, 13) + std::chrono::hours{23} + std::chrono::minutes{59} + std::chrono::seconds{59};
    value.grace_days = 0;
    value.seats = 0;
    value.features = {"support", "pro.*", "enterprise.*", "data_packs"};
    value.machines = {"LAB-RIG-*", "bench-3"};
    value.updates_until = value.expires_at;
    value.replaces = "ADL-2025-000007";
    value.clock_reset = "reset-2026-11-13-a";
    return value;
}

[[nodiscard]] inline std::string signed_payload(std::string_view payload,
                                                std::string_view purpose = licence::licence_purpose,
                                                std::string_view key_id = licence::test_keys::key_id)
{
    return licence::signed_file::write(
        licence::signing::sign(purpose, key_id, span_of(payload), licence::test_keys::private_key()));
}

[[nodiscard]] inline std::string signed_text(const licence::Licence& value,
                                             std::string_view purpose = licence::licence_purpose)
{
    const auto payload = licence::write_payload(value);
    REQUIRE(payload);
    return signed_payload(*payload, purpose);
}

// What the program knows when it judges a licence at `now`, on a computer called LAB-RIG-07 that trusts the
// test key.
[[nodiscard]] inline licence::Context on(licence::TimePoint now)
{
    licence::Context where;
    where.now = now;
    where.computer_name = "LAB-RIG-07";
    where.keys = licence::test_keys::trusted();
    return where;
}

// The payload of `annual()` with one piece of text replaced.
[[nodiscard]] inline std::string payload_with(std::string_view from, std::string_view to)
{
    auto text = licence::write_payload(annual());
    REQUIRE(text);
    const auto position = text->find(from);
    REQUIRE(position != std::string::npos);
    text->replace(position, from.size(), to);
    return *text;
}

inline void expect_payload_error(const std::string& payload, licence::Error expected)
{
    const auto parsed = licence::parse_payload(span_of(payload));
    REQUIRE(!parsed);
    REQUIRE(parsed.error() == expected);
}

} // namespace adl::test
