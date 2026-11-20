// SPDX-License-Identifier: GPL-3.0-or-later
// The command line of adl_mock: what it takes, what it makes of it, and what it refuses.

#include "adl/mock/cli/options.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace adl::mock::cli;
using adl::arinc615a::Address;
using adl::arinc615a::StatusCode;
using adl::mock::OperationKind;

namespace
{

[[nodiscard]] std::expected<Options, UsageError> parse(std::vector<std::string_view> arguments)
{
    return parse_options(arguments);
}

// The message of the error `arguments` give, or an empty string if they are accepted.
[[nodiscard]] std::string refused(std::vector<std::string_view> arguments)
{
    const auto parsed = parse(std::move(arguments));
    return parsed ? std::string{} : parsed.error().message;
}

} // namespace

TEST_CASE("with no options the target is the default one on the loopback address", "[mock_cli][options]")
{
    const auto parsed = parse({});
    REQUIRE(parsed);
    CHECK(parsed->action == Action::run);
    CHECK(parsed->settings.hardware_id == "HNPFMS");
    CHECK(parsed->settings.position == "L");
    CHECK(parsed->settings.type_name == "FMS");
    CHECK(parsed->settings.literal_name == "FMS LEFT");
    CHECK(parsed->settings.manufacturer_code == "HNP");
    CHECK(parsed->settings.tftp_port == 59);
    CHECK(parsed->settings.find_port == 1001);
    CHECK(parsed->settings.think_time_ms == 1000);
    CHECK(parsed->settings.status_period_sec == 4);
    CHECK(parsed->address == Address::loopback());
    CHECK(parsed->parts.empty());
    CHECK_FALSE(parsed->holds.has_value());
    CHECK_FALSE(parsed->storage.has_value());
    CHECK_FALSE(parsed->operations.has_value());
    CHECK_FALSE(parsed->seconds.has_value());
    CHECK_FALSE(parsed->json);
    CHECK(fault_options(*parsed).empty());
}

TEST_CASE("the identity, the files, the network and the pace are set by their options", "[mock_cli][options]")
{
    const auto parsed = parse({"--hardware-id",  "ACMEFMC",   "--position",      "R",        "--type-name",     "FMC",
                               "--literal-name", "FMC RIGHT", "--manufacturer",  "ACM",      "--holds",         "held",
                               "--storage",      "kept",      "--address",       "10.1.2.3", "--tftp-port",     "30959",
                               "--find-port",    "30961",     "--think-time-ms", "250",      "--status-period", "1",
                               "--operations",   "3",         "--seconds",       "60",       "--json"});
    REQUIRE(parsed);
    CHECK(parsed->settings.hardware_id == "ACMEFMC");
    CHECK(parsed->settings.position == "R");
    CHECK(parsed->settings.type_name == "FMC");
    CHECK(parsed->settings.literal_name == "FMC RIGHT");
    CHECK(parsed->settings.manufacturer_code == "ACM");
    CHECK(parsed->holds == std::filesystem::path{"held"});
    CHECK(parsed->storage == std::filesystem::path{"kept"});
    CHECK(parsed->address == Address{{10, 1, 2, 3}});
    CHECK(parsed->settings.tftp_port == 30959);
    CHECK(parsed->settings.find_port == 30961);
    CHECK(parsed->settings.think_time_ms == 250);
    CHECK(parsed->settings.status_period_sec == 1);
    CHECK(parsed->operations == 3);
    CHECK(parsed->seconds == 60);
    CHECK(parsed->json);
}

TEST_CASE("every fault is set by its option and listed as on", "[mock_cli][options]")
{
    const auto parsed = parse({"--wait-first", "7", "--refuse", "download-operator", "--refuse-code", "0x1002",
                               "--invalid-init-file", "--drop-status-every", "3", "--corrupt-served",
                               "--wrong-checksum", "--abort-after", "2", "--silent", "--seconds", "5"});
    REQUIRE(parsed);
    const auto& settings = parsed->settings;
    CHECK(settings.wait_first_seconds == 7);
    CHECK_FALSE(settings.wait_every_request);
    CHECK(settings.refuse_operation == OperationKind::download_operator);
    CHECK(settings.refuse_code == StatusCode::operation_not_supported);
    CHECK(settings.invalid_init_file);
    CHECK(settings.drop_status_every == 3);
    CHECK(settings.corrupt_served_file);
    CHECK(settings.wrong_checksum);
    CHECK(settings.abort_after_statuses == 2);
    CHECK(settings.silent);
    CHECK(fault_options(*parsed) == std::vector<std::string>{"--wait-first", "--refuse", "--refuse-code",
                                                             "--invalid-init-file", "--drop-status-every",
                                                             "--corrupt-served", "--wrong-checksum", "--abort-after",
                                                             "--silent"});
}

TEST_CASE("WAIT on every request is --wait-every, and refusing with 0x1000 needs no code", "[mock_cli][options]")
{
    const auto every = parse({"--wait-every", "2"});
    REQUIRE(every);
    CHECK(every->settings.wait_first_seconds == 2);
    CHECK(every->settings.wait_every_request);
    CHECK(fault_options(*every) == std::vector<std::string>{"--wait-every"});

    const auto refuse = parse({"--refuse", "information"});
    REQUIRE(refuse);
    CHECK(refuse->settings.refuse_operation == OperationKind::information);
    CHECK(refuse->settings.refuse_code == StatusCode::operation_denied);
    CHECK(fault_options(*refuse) == std::vector<std::string>{"--refuse"});

    const auto explicit_denied = parse({"--refuse", "upload", "--refuse-code", "0x1000"});
    REQUIRE(explicit_denied);
    CHECK(explicit_denied->settings.refuse_code == StatusCode::operation_denied);
}

TEST_CASE("each operation can be refused by name", "[mock_cli][options]")
{
    const std::vector<std::pair<std::string_view, OperationKind>> names = {
        {"information", OperationKind::information},
        {"upload", OperationKind::upload},
        {"download-media", OperationKind::download_media},
        {"download-operator", OperationKind::download_operator}};
    for (const auto& [name, kind] : names)
    {
        const auto parsed = parse({"--refuse", name});
        REQUIRE(parsed);
        CHECK(parsed->settings.refuse_operation == kind);
    }
    CHECK_FALSE(refused({"--refuse", "download"}).empty());
    CHECK_FALSE(refused({"--refuse", "Upload"}).empty());
    CHECK_FALSE(refused({"--refuse", ""}).empty());
}

TEST_CASE("--help and --version are actions of their own", "[mock_cli][options]")
{
    const auto help = parse({"--help"});
    REQUIRE(help);
    CHECK(help->action == Action::help);
    const auto version = parse({"--version"});
    REQUIRE(version);
    CHECK(version->action == Action::version);
    // The first of them decides; an error before it is still an error.
    const auto both = parse({"--version", "--help"});
    REQUIRE(both);
    CHECK(both->action == Action::version);
    CHECK_FALSE(refused({"--nonsense", "--help"}).empty());
}

TEST_CASE("--scenario-actions is an action of its own, like --help", "[mock_cli][options][scenario]")
{
    const auto actions = parse({"--scenario-actions"});
    REQUIRE(actions);
    CHECK(actions->action == Action::scenario_actions);
    // The first of the actions decides, as for --help and --version; an error before it is still an error.
    CHECK(parse({"--help", "--scenario-actions"})->action == Action::help);
    CHECK(parse({"--scenario-actions", "--help"})->action == Action::scenario_actions);
    CHECK_FALSE(refused({"--nonsense", "--scenario-actions"}).empty());
    // It says what it is in the usage text.
    CHECK(usage_text().contains("--scenario-actions"));
}

TEST_CASE("the parts are grouped by hardware and given serial numbers", "[mock_cli][options]")
{
    const auto parsed = parse(
        {"--part", "FMS LEFT:ACM47-1:A:FMS 1 OPS", "--part", "FMS LEFT:ACM47-2", "--part", "NAV:ACM47-3::NAV DATA"});
    REQUIRE(parsed);
    REQUIRE(parsed->parts.size() == 2);
    CHECK(parsed->parts[0].literal_name == "FMS LEFT");
    CHECK(parsed->parts[0].serial_number == "SN-0001");
    REQUIRE(parsed->parts[0].parts.size() == 2);
    CHECK(parsed->parts[0].parts[0].part_number == "ACM47-1");
    CHECK(parsed->parts[0].parts[0].amendment == "A");
    CHECK(parsed->parts[0].parts[0].designation == "FMS 1 OPS");
    CHECK(parsed->parts[0].parts[1].part_number == "ACM47-2");
    CHECK(parsed->parts[0].parts[1].amendment.empty());
    CHECK(parsed->parts[0].parts[1].designation == "FMS LEFT"); // none given: the hardware's own name
    CHECK(parsed->parts[1].literal_name == "NAV");
    CHECK(parsed->parts[1].serial_number == "SN-0002");
    CHECK(parsed->parts[1].parts[0].amendment.empty());
    CHECK(parsed->parts[1].parts[0].designation == "NAV DATA");
}

TEST_CASE("a part that does not fit the form or the limits is refused", "[mock_cli][options]")
{
    for (const std::string_view part : {"", "FMS", "FMS:", ":P1", "A:B:C:D:E", "FMS:P1:A:", "FMS:P\x01"})
    {
        INFO(part);
        CHECK(refused({"--part", part}).contains("--part"));
    }
    CHECK_FALSE(refused({"--part", std::string(255, 'H') + ":P1"}).empty());
    CHECK_FALSE(refused({"--part", "H:" + std::string(255, 'P')}).empty());
    CHECK(refused({"--part", std::string(254, 'H') + ":" + std::string(254, 'P')}).empty());
}

TEST_CASE("a number outside its range, or that is not a plain number, is refused naming the option",
          "[mock_cli][options]")
{
    struct Case
    {
        std::string_view option;
        std::string_view low;
        std::string_view high;
        std::string_view too_low;
        std::string_view too_high;
    };
    const std::vector<Case> cases = {
        {"--tftp-port", "1", "65535", "0", "65536"},
        {"--find-port", "1", "65535", "0", "65536"},
        {"--think-time-ms", "0", "60000", "-1", "60001"},
        {"--status-period", "0", "3600", "-1", "3601"},
        {"--wait-first", "1", "65535", "0", "65536"},
        {"--wait-every", "1", "65535", "0", "65536"},
        {"--drop-status-every", "1", "1000000", "0", "1000001"},
        {"--abort-after", "1", "1000000", "0", "1000001"},
        {"--operations", "1", "1000000000", "0", "1000000001"},
        {"--seconds", "1", "31536000", "0", "31536001"},
    };
    for (const Case& test : cases)
    {
        INFO(test.option);
        // --silent needs --seconds, so give one to the options that are not --seconds itself.
        const auto accepted = [&test](std::string_view value)
        {
            std::vector<std::string_view> arguments = {test.option, value};
            if (test.option != "--seconds")
            {
                arguments.insert(arguments.end(), {"--seconds", "1"});
            }
            return refused(arguments).empty();
        };
        CHECK(accepted(test.low));
        CHECK(accepted(test.high));
        CHECK_FALSE(accepted(test.too_low));
        CHECK_FALSE(accepted(test.too_high));
        for (const std::string_view text :
             {"", "x", "1x", " 1", "1 ", "+1", "1.5", "0x10", "-0", "99999999999999999999"})
        {
            INFO(text);
            CHECK(refused({test.option, text}).contains(test.option));
        }
    }
}

TEST_CASE("an address is four numbers from 0 to 255 with dots", "[mock_cli][options]")
{
    CHECK(parse_ipv4("127.0.0.1") == Address::loopback());
    CHECK(parse_ipv4("0.0.0.0") == Address{{0, 0, 0, 0}});
    CHECK(parse_ipv4("255.255.255.255") == Address::broadcast());
    CHECK(parse_ipv4("10.20.30.40") == Address{{10, 20, 30, 40}});
    for (const std::string_view text :
         {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.-4", "1.2.3.", ".1.2.3", "a.b.c.d", "01.2.3.4", "1.2.3.04",
          "1..2.3", "1.2.3.4 ", "::1", "localhost", "1.2.3.1000"})
    {
        INFO(text);
        CHECK_FALSE(parse_ipv4(text).has_value());
    }
    CHECK(format_ipv4(Address{{10, 20, 30, 40}}) == "10.20.30.40");
    CHECK(format_ipv4(Address::loopback()) == "127.0.0.1");
    CHECK(refused({"--address", "localhost"}).contains("--address"));
}

TEST_CASE("the identity must fit what FIND carries and what a file name may hold", "[mock_cli][options]")
{
    CHECK(refused({"--hardware-id", std::string(15, 'H')}).empty());
    CHECK(refused({"--hardware-id", std::string(16, 'H')}).contains("--hardware-id"));
    CHECK(refused({"--hardware-id", ""}).contains("--hardware-id"));
    CHECK(refused({"--hardware-id", "HNP FMS"}).contains("--hardware-id")); // part of file names: no space
    CHECK(refused({"--hardware-id", "HNP/FMS"}).contains("--hardware-id"));
    CHECK(refused({"--hardware-id", "HNP:FMS"}).contains("--hardware-id"));
    CHECK(refused({"--position", std::string(8, 'P')}).empty());
    CHECK(refused({"--position", std::string(9, 'P')}).contains("--position"));
    CHECK(refused({"--position", ""}).empty()); // no position: THW_ID_POS is the hardware ID
    CHECK(refused({"--type-name", std::string(8, 'T')}).empty());
    CHECK(refused({"--type-name", std::string(9, 'T')}).contains("--type-name"));
    CHECK(refused({"--literal-name", std::string(20, 'L')}).empty());
    CHECK(refused({"--literal-name", std::string(21, 'L')}).contains("--literal-name"));
    CHECK(refused({"--literal-name", ""}).contains("--literal-name"));
    CHECK(refused({"--literal-name", "FMS LEFT"}).empty()); // a space is allowed here
    CHECK(refused({"--manufacturer", "ABC"}).empty());
    CHECK(refused({"--manufacturer", "ABCD"}).contains("--manufacturer"));
    CHECK(refused({"--manufacturer", "caf\xC3\xA9"}).contains("--manufacturer")); // ASCII only
    // THW_ID_POS names the protocol files, so it must be a name every system can give a file.
    CHECK(refused({"--hardware-id", "CON", "--position", ""}).contains("file"));
    CHECK(refused({"--hardware-id", "HNPFMS.", "--position", ""}).contains("file"));
}

TEST_CASE("options that cannot go together or that need another are refused", "[mock_cli][options]")
{
    CHECK(refused({"--wait-first", "1", "--wait-every", "1"}).contains("--wait-first and --wait-every"));
    CHECK(refused({"--refuse-code", "0x1002"}).contains("--refuse-code needs --refuse"));
    CHECK(refused({"--silent"}).contains("--silent needs --seconds"));
    CHECK(refused({"--silent", "--operations", "1"}).contains("--silent needs --seconds"));
    CHECK(refused({"--silent", "--seconds", "5"}).empty());
    CHECK(refused({"--refuse-code", "0x1001", "--refuse", "upload"}).contains("--refuse-code"));
    CHECK(refused({"--refuse-code", "4098", "--refuse", "upload"}).contains("--refuse-code"));
}

TEST_CASE("an option given twice, a missing value, an unknown option and a stray argument are refused",
          "[mock_cli][options]")
{
    CHECK(refused({"--json", "--json"}).contains("given twice"));
    CHECK(refused({"--hardware-id", "A", "--hardware-id", "B"}).contains("--hardware-id is given twice"));
    CHECK(refused({"--seconds"}).contains("--seconds needs a value"));
    CHECK(refused({"--holds"}).contains("needs a value"));
    CHECK(refused({"--no-such-option"}).contains("unknown option --no-such-option"));
    CHECK(refused({"-j"}).contains("unexpected argument -j"));
    CHECK(refused({"stray"}).contains("unexpected argument stray"));
    CHECK(refused({"--holds", ""}).contains("--holds needs a folder"));
    CHECK(refused({"--storage", ""}).contains("--storage needs a folder"));
    CHECK(refused({"--hardware-id=A"}).contains("unknown option"));   // options are --name value
    CHECK(refused({"--wait-first", "1,2"}).contains("--wait-first")); // never a list with commas
}

TEST_CASE("a value is taken as it is, whatever it looks like", "[mock_cli][options]")
{
    const auto parsed = parse({"--holds", "--json"});
    REQUIRE(parsed);
    CHECK(parsed->holds == std::filesystem::path{"--json"});
    CHECK_FALSE(parsed->json);
}

TEST_CASE("the usage text names every option the program takes", "[mock_cli][options]")
{
    const std::string usage = usage_text();
    for (const std::string_view option : {"--hardware-id",
                                          "--position",
                                          "--type-name",
                                          "--literal-name",
                                          "--manufacturer",
                                          "--part",
                                          "--holds",
                                          "--storage",
                                          "--address",
                                          "--tftp-port",
                                          "--find-port",
                                          "--think-time-ms",
                                          "--status-period",
                                          "--wait-first",
                                          "--wait-every",
                                          "--refuse",
                                          "--refuse-code",
                                          "--invalid-init-file",
                                          "--drop-status-every",
                                          "--corrupt-served",
                                          "--wrong-checksum",
                                          "--abort-after",
                                          "--silent",
                                          "--operations",
                                          "--seconds",
                                          "--json",
                                          "--help",
                                          "--version"})
    {
        INFO(option);
        CHECK(usage.contains(option));
    }
    CHECK(usage.starts_with("usage: adl_mock"));
}

TEST_CASE("--scenario names the file that says which fault happens when", "[mock_cli][options][scenario]")
{
    const auto parsed = parse({"--scenario", "rig/scenario.json", "--seconds", "60", "--json"});
    REQUIRE(parsed);
    REQUIRE(parsed->scenario.has_value());
    CHECK(*parsed->scenario == std::filesystem::path("rig/scenario.json"));
    CHECK(fault_options(*parsed) == std::vector<std::string>{"--scenario"});
    CHECK_FALSE(parse({})->scenario.has_value());

    CHECK(refused({"--scenario"}) == "--scenario needs a value");
    CHECK(refused({"--scenario", "a.json", "--scenario", "b.json"}) == "--scenario is given twice");
    CHECK(refused({"--scenario", ""}) == "--scenario needs a file"); // an empty path is not a file
}

TEST_CASE("--scenario cannot be used with an option that sets a fault, which is named", "[mock_cli][options][scenario]")
{
    const std::pair<std::vector<std::string_view>, std::string_view> faults[] = {
        {{"--wait-first", "2"}, "--wait-first"},
        {{"--wait-every", "2"}, "--wait-every"},
        {{"--refuse", "upload"}, "--refuse"},
        {{"--refuse", "upload", "--refuse-code", "0x1002"}, "--refuse"},
        {{"--invalid-init-file"}, "--invalid-init-file"},
        {{"--drop-status-every", "3"}, "--drop-status-every"},
        {{"--corrupt-served"}, "--corrupt-served"},
        {{"--wrong-checksum"}, "--wrong-checksum"},
        {{"--abort-after", "2"}, "--abort-after"},
        {{"--silent", "--seconds", "5"}, "--silent"},
    };
    for (const auto& [options, named] : faults)
    {
        std::vector<std::string_view> arguments{"--scenario", "s.json"};
        arguments.insert(arguments.end(), options.begin(), options.end());
        INFO(named);
        CHECK(refused(arguments) ==
              "--scenario cannot be used with " + std::string{named} + ": the scenario sets the faults");
        // In the other order too.
        std::vector<std::string_view> before = options;
        before.insert(before.end(), {"--scenario", "s.json"});
        CHECK(refused(before) ==
              "--scenario cannot be used with " + std::string{named} + ": the scenario sets the faults");
    }
    // What is not a fault is free: the identity, the files, the network, the pace, the stop and the output.
    const auto parsed =
        parse({"--scenario", "s.json", "--hardware-id", "X1", "--holds", "h", "--storage", "s", "--address", "0.0.0.0",
               "--think-time-ms", "5", "--status-period", "2", "--operations", "3", "--seconds", "9", "--json"});
    REQUIRE(parsed);
    CHECK(parsed->scenario.has_value());
}

TEST_CASE("the usage text says what --scenario is, and the exit code 5", "[mock_cli][options][scenario]")
{
    const std::string text = usage_text();
    CHECK(text.contains("--scenario <file>"));
    CHECK(text.contains("5 a scenario file cannot be used"));
}
