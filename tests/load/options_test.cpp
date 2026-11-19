// SPDX-License-Identifier: GPL-3.0-or-later
// The command line of adl_load: every option of every command, and every kind of wrong usage. The parser
// reads nothing but the arguments, so none of this needs a host.

#include "adl/load/options.hpp"

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using namespace adl::load;

namespace
{

// The options a command line gives, which has to be accepted.
Options accepted(std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> list(arguments);
    const auto parsed = parse_options(list);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->action == Action::run);
    return parsed->options;
}

// What a command line is refused with.
std::string refused(std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> list(arguments);
    const auto parsed = parse_options(list);
    REQUIRE_FALSE(parsed.has_value());
    return parsed.error();
}

bool mentions(const std::string& text, std::string_view word)
{
    return text.find(word) != std::string::npos;
}

// The arguments of a command followed by more, for the cases that build on one base.
std::expected<Parsed, std::string> parse_with(std::vector<std::string_view> base,
                                              std::initializer_list<std::string_view> more)
{
    base.insert(base.end(), more.begin(), more.end());
    return parse_options(base);
}

constexpr std::string_view target = "10.0.0.2";
constexpr std::string_view position = "HNPFMS_L";

} // namespace

TEST_CASE("find: the window, the interface, and --json", "[load][options]")
{
    const Options plain = accepted({"find"});
    CHECK(plain.command == Command::find);
    CHECK(plain.seconds == default_find_seconds);
    CHECK(plain.seconds == 3);
    CHECK_FALSE(plain.local_address.has_value());
    CHECK_FALSE(plain.json);

    const Options all = accepted({"find", "--seconds", "10", "--local-address", "192.168.1.5", "--json"});
    CHECK(all.seconds == 10);
    CHECK(all.local_address == adl::loader_core::Ipv4Octets{192, 168, 1, 5});
    CHECK(all.json);

    CHECK(accepted({"find", "--seconds", "3600"}).seconds == max_find_seconds);
    CHECK(mentions(refused({"find", "--seconds", "0"}), "--seconds takes a whole number from 1 to 3600"));
    CHECK(mentions(refused({"find", "--seconds", "3601"}), "--seconds"));
    CHECK(mentions(refused({"find", "--seconds", "three"}), "\"three\""));
    CHECK(mentions(refused({"find", "--seconds", "-1"}), "--seconds"));
    CHECK(mentions(refused({"find", "--seconds", "5s"}), "--seconds"));
    CHECK(mentions(refused({"find", "--seconds", "1", "--seconds", "2"}), "--seconds is given twice"));
    CHECK(mentions(refused({"find", "--local-address", "localhost"}), "IPv4 address"));
    CHECK(mentions(refused({"find", "--local-address", "1.2.3"}), "\"1.2.3\""));
}

TEST_CASE("an Information operation takes a target, a position and the common options", "[load][options]")
{
    const Options minimal = accepted({"information", "--target", target, "--position", position});
    CHECK(minimal.command == Command::information);
    CHECK(minimal.target == adl::loader_core::Ipv4Octets{10, 0, 0, 2});
    CHECK(minimal.position == "HNPFMS_L");
    CHECK(minimal.control_port == 59);
    CHECK_FALSE(minimal.local_address.has_value());
    CHECK_FALSE(minimal.record.has_value());
    CHECK_FALSE(minimal.json);

    const Options all = accepted({"information", "--json", "--record", "out/rec.json", "--control-port", "30901",
                                  "--local-address", "127.0.0.1", "--position", position, "--target", "127.0.0.1"});
    CHECK(all.json);
    CHECK(all.record == "out/rec.json");
    CHECK(all.control_port == 30901);
    CHECK(all.local_address == adl::loader_core::Ipv4Octets{127, 0, 0, 1});
    CHECK(all.target == adl::loader_core::Ipv4Octets{127, 0, 0, 1});

    CHECK(
        accepted({"information", "--target", target, "--position", position, "--control-port", "65535"}).control_port ==
        65535);
    CHECK(mentions(refused({"information", "--position", position}), "--target is missing"));
    CHECK(mentions(refused({"information", "--target", target}), "--position is missing"));
    CHECK(mentions(refused({"information", "--target", "10.0.0", "--position", position}), "\"10.0.0\""));
    CHECK(mentions(refused({"information", "--target", target, "--position", position, "--control-port", "0"}),
                   "--control-port"));
    CHECK(mentions(refused({"information", "--target", target, "--position", position, "--control-port", "65536"}),
                   "--control-port"));
    CHECK(mentions(refused({"information", "--target", target, "--target", target, "--position", position}),
                   "--target is given twice"));
    CHECK(
        mentions(refused({"information", "--target", target, "--position", position, "--record", "a", "--record", "b"}),
                 "--record is given twice"));
}

TEST_CASE("the port the target takes the first request on is the standard's unless it is said", "[load][options]")
{
    CHECK(accepted({"information", "--target", target, "--position", position}).target_port == 59);
    CHECK(accepted({"information", "--target", target, "--position", position, "--target-port", "30959"}).target_port ==
          30959);
    CHECK(accepted({"information", "--target", target, "--position", position, "--target-port", "1"}).target_port == 1);
    CHECK(accepted({"information", "--target", target, "--position", position, "--target-port", "65535"}).target_port ==
          65535);
    // The target's port and the loader's are two options, and one does not set the other.
    const Options both = accepted({"information", "--target", target, "--position", position, "--control-port", "30901",
                                   "--target-port", "30959"});
    CHECK(both.control_port == 30901);
    CHECK(both.target_port == 30959);
    CHECK(
        accepted({"information", "--target", target, "--position", position, "--control-port", "30901"}).target_port ==
        59);

    // An upload and a download have it too.
    CHECK(accepted({"upload", "--target", target, "--position", position, "--media-set", "set", "--all-loads",
                    "--target-port", "30959"})
              .target_port == 30959);
    CHECK(accepted({"download", "--target", target, "--position", position, "--into", "out", "--pick-all",
                    "--target-port", "30959"})
              .target_port == 30959);

    // 0 and what is over 65535 are wrong usage, and so is what is not a number.
    for (const char* bad : {"0", "65536", "100000", "-1", "59a", "port", "", " 59"})
    {
        CAPTURE(bad);
        const std::vector<std::string_view> list{"information", "--target",      target, "--position",
                                                 position,      "--target-port", bad};
        const auto parsed = parse_options(list);
        REQUIRE_FALSE(parsed.has_value());
        CHECK((mentions(parsed.error(), "--target-port takes a whole number from 1 to 65535") ||
               mentions(parsed.error(), "--target-port needs a value")));
    }
    CHECK(mentions(refused({"information", "--target", target, "--position", position, "--target-port", "59",
                            "--target-port", "60"}),
                   "--target-port is given twice"));
    // find has no target.
    CHECK(mentions(refused({"find", "--target-port", "59"}), "--target-port is not an option of find"));
}

TEST_CASE("a position has to be one the loader can use", "[load][options]")
{
    CHECK(mentions(refused({"information", "--target", target, "--position", "A/B"}), "--position \"A/B\""));
    CHECK(mentions(refused({"information", "--target", target, "--position", "A\\B"}), "is not a THW_ID_POS"));
    const std::string too_long(241, 'A');
    CHECK(mentions(refused({"information", "--target", target, "--position", too_long}), "1 to 240 characters"));
    const std::string longest(240, 'A');
    CHECK(accepted({"information", "--target", target, "--position", longest}).position.size() == 240);
    // A position that cannot name a folder is refused for a download alone.
    CHECK(accepted({"information", "--target", target, "--position", "A:B"}).position == "A:B");
    CHECK(mentions(refused({"download", "--target", target, "--position", "A:B", "--into", "out", "--pick-all"}),
                   "a download can be made for"));
}

TEST_CASE("an upload names its loads or takes them all", "[load][options]")
{
    const Options some = accepted({"upload", "--target", target, "--position", position, "--media-set", "set", "--load",
                                   "PN-1", "--load", "PN-2"});
    CHECK(some.command == Command::upload);
    CHECK(some.media_set == "set");
    CHECK(some.loads == std::vector<std::string>{"PN-1", "PN-2"});
    CHECK_FALSE(some.all_loads);

    const Options all = accepted({"upload", "--target", target, "--position", position, "--media-set", "set",
                                  "--all-loads", "--json", "--record", "r.json"});
    CHECK(all.all_loads);
    CHECK(all.loads.empty());

    const std::vector<std::string_view> base{"upload", "--target", target, "--position", position};
    CHECK(mentions(parse_with(base, {"--load", "PN"}).error(), "--media-set is missing"));
    CHECK(mentions(parse_with(base, {"--media-set", "set"}).error(), "Name the loads"));
    CHECK(mentions(parse_with(base, {"--media-set", "set", "--load", "PN", "--all-loads"}).error(),
                   "cannot be given together"));
    CHECK(mentions(parse_with(base, {"--media-set", "set", "--load", "PN", "--load", "PN"}).error(),
                   "\"PN\" is given twice"));
    CHECK(mentions(parse_with(base, {"--media-set", "a", "--media-set", "b", "--all-loads"}).error(),
                   "--media-set is given twice"));
    CHECK(mentions(parse_with(base, {"--media-set", "set", "--all-loads", "--all-loads"}).error(),
                   "--all-loads is given twice"));
    CHECK(mentions(parse_with(base, {"--media-set", "set", "--all-loads", "--into", "x"}).error(),
                   "--into is not an option of upload"));
    CHECK(mentions(parse_with(base, {"--media-set", "set", "--all-loads", "--file", "x"}).error(),
                   "--file is not an option of upload"));
}

TEST_CASE("a download names its files, picks from the target's list, or takes all of it", "[load][options]")
{
    const Options named = accepted({"download", "--target", target, "--position", position, "--into", "out", "--file",
                                    "A.BIN", "--file", "B.BIN"});
    CHECK(named.command == Command::download);
    CHECK(named.into == "out");
    CHECK(named.choice == FileChoice::named);
    CHECK(named.names == std::vector<std::string>{"A.BIN", "B.BIN"});
    CHECK_FALSE(named.pick_all);

    const Options picked = accepted({"download", "--target", target, "--position", position, "--into", "out", "--pick",
                                     "A.BIN", "--pick", "C.BIN"});
    CHECK(picked.choice == FileChoice::picked);
    CHECK(picked.names == std::vector<std::string>{"A.BIN", "C.BIN"});
    CHECK_FALSE(picked.pick_all);

    const Options everything =
        accepted({"download", "--target", target, "--position", position, "--into", "out", "--pick-all"});
    CHECK(everything.choice == FileChoice::picked);
    CHECK(everything.pick_all);
    CHECK(everything.names.empty());

    const std::vector<std::string_view> base{"download", "--target", target, "--position", position};
    CHECK(mentions(parse_with(base, {"--file", "A"}).error(), "--into is missing"));
    CHECK(mentions(parse_with(base, {"--into", "out"}).error(), "Name the files to download"));
    CHECK(mentions(parse_with(base, {"--into", "out", "--file", "A", "--pick", "B"}).error(),
                   "cannot be given together"));
    CHECK(
        mentions(parse_with(base, {"--into", "out", "--pick", "A", "--pick-all"}).error(), "cannot be given together"));
    CHECK(
        mentions(parse_with(base, {"--into", "out", "--file", "A", "--pick-all"}).error(), "cannot be given together"));
    CHECK(mentions(parse_with(base, {"--into", "out", "--file", "A", "--file", "A"}).error(),
                   "--file \"A\" is given twice"));
    CHECK(mentions(parse_with(base, {"--into", "out", "--pick", "A", "--pick", "A"}).error(),
                   "--pick \"A\" is given twice"));
    CHECK(mentions(parse_with(base, {"--into", "a", "--into", "b", "--pick-all"}).error(), "--into is given twice"));
    CHECK(mentions(parse_with(base, {"--into", "out", "--pick-all", "--media-set", "x"}).error(),
                   "--media-set is not an option of download"));
    CHECK(mentions(parse_with(base, {"--into", "out", "--pick-all", "--all-loads"}).error(),
                   "--all-loads is not an option of download"));
}

TEST_CASE("options that do not belong to a command are refused", "[load][options]")
{
    CHECK(mentions(refused({"find", "--target", target}), "--target is not an option of find"));
    CHECK(mentions(refused({"find", "--record", "r.json"}), "--record is not an option of find"));
    CHECK(mentions(refused({"find", "--control-port", "59"}), "--control-port is not an option of find"));
    CHECK(mentions(refused({"information", "--target", target, "--position", position, "--seconds", "3"}),
                   "--seconds is not an option of information"));
    CHECK(mentions(refused({"information", "--target", target, "--position", position, "--pick-all"}),
                   "--pick-all is not an option of information"));
}

TEST_CASE("the first argument is a command, --help or --version", "[load][options]")
{
    CHECK(mentions(refused({}), "No command given"));
    CHECK(mentions(refused({"upload-all"}), "Unknown command \"upload-all\""));
    CHECK(mentions(refused({"--json"}), "The first argument has to be a command"));
    CHECK(mentions(refused({"--target", target}), "has to be a command"));

    const std::vector<std::string_view> help{"--help"};
    CHECK(parse_options(help)->action == Action::help);
    const std::vector<std::string_view> version{"--version"};
    CHECK(parse_options(version)->action == Action::version);
    const std::vector<std::string_view> command_help{"upload", "--help"};
    CHECK(parse_options(command_help)->action == Action::help);
    const std::vector<std::string_view> command_version{"information", "--target", "10.0.0.2", "--version", "--bogus"};
    CHECK(parse_options(command_version)->action == Action::version);
    // What is wrong first is what is reported, ahead of a --help after it.
    CHECK(mentions(refused({"information", "--bogus", "--help"}), "Unknown option \"--bogus\""));
}

TEST_CASE("a stray word, an unknown option and a missing value are told apart", "[load][options]")
{
    CHECK(mentions(refused({"information", "10.0.0.2"}), "Unexpected argument \"10.0.0.2\""));
    CHECK(mentions(refused({"find", "--bogus"}), "Unknown option \"--bogus\""));
    CHECK(mentions(refused({"find", "-j"}), "Unexpected argument \"-j\""));
    CHECK(mentions(refused({"information", "--target=10.0.0.2"}), "(write --target 10.0.0.2, with a space)"));
    CHECK(mentions(refused({"information", "--target"}), "--target needs a value"));
    CHECK(mentions(refused({"information", "--target", "--position", position}), "--target needs a value"));
    CHECK(mentions(refused({"information", "--target", ""}), "--target needs a value"));
    CHECK(mentions(refused({"upload", "--target", target, "--position", position, "--media-set", "--all-loads"}),
                   "--media-set needs a value"));
    // A value that has to begin with two dashes is written with a path in front of it.
    CHECK(accepted({"information", "--target", target, "--position", position, "--record", "./--r.json"}).record ==
          "./--r.json");
}

TEST_CASE("the usage text names every command and option, and the version line the events format", "[load][options]")
{
    const std::string_view usage = usage_text();
    for (const std::string_view word : {"adl_load find",   "adl_load information",
                                        "adl_load upload", "adl_load download",
                                        "--seconds",       "--local-address",
                                        "--target",        "--position",
                                        "--media-set",     "--load",
                                        "--all-loads",     "--into",
                                        "--file",          "--pick",
                                        "--pick-all",      "--control-port",
                                        "--json",          "--record",
                                        "--help",          "--version",
                                        "Exit codes"})
    {
        CAPTURE(word);
        CHECK(usage.find(word) != std::string_view::npos);
    }
    const std::string version = version_line();
    CHECK(version.starts_with("adl_load "));
    CHECK(version.ends_with(" events 1"));
    CHECK(version.find(".exe") == std::string::npos);
}
