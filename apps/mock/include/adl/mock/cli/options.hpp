// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The command line of adl_mock: what it may say, and what it makes of what it is given. Nothing here
// opens a socket or a folder; a program that reads the options and does nothing else can be tested
// without either.

#include "adl/arinc615a/endpoint.hpp"
#include "adl/mock/engine.hpp"
#include "adl/mock/settings.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::mock::cli
{

enum class Action : std::uint8_t
{
    run,
    help,
    version,
};

struct Options
{
    Action action = Action::run;

    // The identity, the ports, the pace and the faults, in the form the window's model also hands
    // to adl::mock::make_config. loopback_only is not used: the address below is.
    TargetSettings settings;
    adl::arinc615a::Address address = adl::arinc615a::Address::loopback();

    // What it reports and what it holds. Empty: the engine's own list, and one demonstration file.
    std::vector<HardwareItem> parts;
    std::optional<std::filesystem::path> holds;
    std::optional<std::filesystem::path> storage;

    // The scenario file that says which fault happens when, in place of the fault options.
    std::optional<std::filesystem::path> scenario;

    // When it stops. Neither: it runs until it is killed.
    std::optional<std::uint64_t> operations;
    std::optional<std::uint64_t> seconds;

    bool json = false;
};

// Why the options cannot be used: one sentence for a person, naming the option.
struct UsageError
{
    std::string message;
};

// The arguments after the program's name. --help and --version give an Options whose action says
// so; every other option is then not looked at beyond the first error.
[[nodiscard]] std::expected<Options, UsageError> parse_options(std::span<const std::string_view> arguments);

// The fault options that are on, as written (--silent, --wait-first), in the order of the usage text.
[[nodiscard]] std::vector<std::string> fault_options(const Options& options);

// The usage text, for --help and for a usage error.
[[nodiscard]] std::string usage_text();

// "127.0.0.1" as an address, or nothing: four decimal numbers from 0 to 255 with dots between and no
// leading zeros.
[[nodiscard]] std::optional<adl::arinc615a::Address> parse_ipv4(std::string_view text);

// An address as four decimal numbers with dots between.
[[nodiscard]] std::string format_ipv4(const adl::arinc615a::Address& address);

} // namespace adl::mock::cli
