// SPDX-License-Identifier: GPL-3.0-or-later
// One call into each library of the development package, so that a missing header, a library that
// was not installed or a link dependency the package config forgot to carry fails the build or the
// run.

#include "adl/arinc615a/checksum.hpp"
#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc665/crc.hpp"
#include "adl/loader_core/media_source.hpp"
#include "adl/loader_core/record_json.hpp"
#include "adl/loader_core/recorder.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/platform/host.hpp"

#include <cstddef>
#include <iostream>
#include <span>
#include <string>
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

    // adl::loader_core: an index of the files of a media set that lists none holds none.
    const auto index = adl::loader_core::MediaSetIndex::make("/set", adl::arinc665::FilesList{});
    ok &= check(index.has_value() && index->entries().empty(), "an empty media set index");

    // adl::loader_core: a recorder logs what the engine reports, and the record is written as the file
    // other programs read.
    adl::loader_core::OperationRecorder recorder{
        adl::arinc615a::OperationKind::information, "10.0.0.2", "HNPFMS_L", {}};
    recorder.on_notice(adl::arinc615a::Notice::waiting, "A.LUI", 5);
    const std::string record = adl::loader_core::record_to_json(recorder.record());
    ok &= check(record.starts_with("{\n  \"format\": 1,\n") && record.contains("target asked to wait: A.LUI (5 s)"),
                "an operation record is written");

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
