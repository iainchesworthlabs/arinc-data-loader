// SPDX-License-Identifier: GPL-3.0-or-later
// One call into each library of the development package, so that a missing header, a library that
// was not installed or a link dependency the package config forgot to carry fails the build or the
// run.

#include "adl/arinc615a/checksum.hpp"
#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc665/crc.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/platform/host.hpp"

#include <cstddef>
#include <iostream>
#include <span>
#include <string_view>

namespace
{

bool check(bool condition, std::string_view what)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << what << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    bool ok = true;

    // adl::arinc665: the CRC-32 of section 4.3 (not reflected, initial value and final XOR all
    // ones) of the conventional test input.
    constexpr std::string_view digits = "123456789";
    const auto bytes = std::as_bytes(std::span{digits.data(), digits.size()});
    ok &= check(adl::arinc665::crc_of<adl::arinc665::Crc32>(bytes) == 0xFC891918U, "CRC-32 check value");

    // adl::media_tree: the standard's members are directories named after their sequence number.
    ok &= check(adl::media_tree::member_directory_name(1) == "member_001", "member directory name");

    // adl::arinc615a_runner (and, through it, adl::arinc615a): a file name a target may ask for.
    ok &= check(adl::arinc615a::is_safe_file_name("LOADS.LUM"), "a plain file name is safe");
    ok &= check(!adl::arinc615a::is_safe_file_name("../LOADS.LUM"), "a parent directory is not");

    // adl::platform: this host's implementation of the interfaces starts.
    auto host = adl::platform::make_host();
    ok &= check(host.has_value(), "make_host");
    if (host.has_value())
    {
        ok &= check(std::string_view{(*host)->info().name}.size() > 0, "the host has a name");
        std::cout << "host: " << (*host)->info().name << '\n';
    }

    return ok ? 0 : 1;
}
