// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "commands.hpp"

#include "adl/licence/licence.hpp"
#include "adl/licence/product.hpp"
#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/test_keys.hpp"
#include "adl/licence/words.hpp"
#include "files.hpp"
#include "messages.hpp"
#include "options.hpp"
#include "public_key.hpp"
#include "report.hpp"
#include "signing.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace adl::licence_owner
{

namespace lic = adl::licence;
namespace sf = adl::licence::signed_file;
namespace fs = std::filesystem;

namespace
{

// A public key file is one line of about 150 characters.
constexpr std::size_t max_public_key_file_size = 4096;

[[nodiscard]] std::span<const std::uint8_t> as_bytes(std::string_view text) noexcept
{
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

[[nodiscard]] std::string hex(std::span<const std::uint8_t> bytes)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes)
    {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

// The first line of the output of a command that was given --test.
void say_test_note(const Console& console, const Options& options)
{
    if (options.has("--test"))
    {
        console.out << message::test_note << '\n';
    }
}

// The keys a check may use: this build's own, the test key for --test, and each --trust file.
[[nodiscard]] std::expected<std::vector<sf::TrustedKey>, std::string> collect_keys(const Options& options)
{
    std::vector<sf::TrustedKey> keys = sf::product_trust_list();
    if (options.has("--test"))
    {
        const auto test = lic::test_keys::trusted();
        keys.insert(keys.end(), test.begin(), test.end());
    }
    for (const std::string& name : options.values("--trust"))
    {
        const auto text = read_file(path_from_utf8(name), max_public_key_file_size);
        if (!text)
        {
            return std::unexpected(format(message::trust_refused, {name, text.error()}));
        }
        auto key = parse_public_key(*text);
        if (!key)
        {
            return std::unexpected(format(message::trust_refused, {name, key.error()}));
        }
        keys.push_back(std::move(*key));
    }
    return keys;
}

// The time an option holds, nothing when it was not given, or the sentence for a usage error.
[[nodiscard]] std::expected<std::optional<lic::TimePoint>, std::string> time_option(const Options& options,
                                                                                   std::string_view name)
{
    if (!options.has(name))
    {
        return std::optional<lic::TimePoint>{};
    }
    const auto parsed = lic::parse_time(options.value(name));
    if (!parsed)
    {
        return std::unexpected(format(message::bad_time, {name}));
    }
    return std::optional<lic::TimePoint>{*parsed};
}

// Prints Format, Purpose and Key lines.
void print_envelope(std::ostream& out, const sf::Envelope& file)
{
    out << "Format: " << file.format << '\n';
    out << "Purpose: " << file.purpose << '\n';
    out << "Key: " << file.key_id << '\n';
    if (file.key_id.starts_with(sf::test_key_prefix))
    {
        out << "Test key: yes\n";
    }
}

// What showing a file found out.
struct Shown
{
    bool payload_read = false;
    bool signature_verifies = false;
};

// Shows what a licence file or a revocation list says and how its signature stands.
[[nodiscard]] Shown show_signed_file(std::ostream& out, const sf::Envelope& file, std::span<const sf::TrustedKey> keys)
{
    print_envelope(out, file);
    Shown shown;
    if (file.purpose == lic::licence_purpose)
    {
        const auto value = lic::parse_payload(file.payload);
        const SignatureCheck signature = check_signature(file, file.purpose, keys, value ? &value->issued_at : nullptr);
        print_signature(out, signature);
        shown.signature_verifies = signature.kind == SignatureCheck::Kind::verifies;
        if (!value)
        {
            out << "Payload: " << format(message::payload_unreadable, {lic::to_string(value.error())}) << '\n';
            return shown;
        }
        print_licence(out, *value);
        shown.payload_read = true;
        return shown;
    }
    if (file.purpose == lic::revocations_purpose)
    {
        const auto list = lic::parse_revocations(file.payload);
        const SignatureCheck signature = check_signature(file, file.purpose, keys, list ? &list->issued_at : nullptr);
        print_signature(out, signature);
        shown.signature_verifies = signature.kind == SignatureCheck::Kind::verifies;
        if (!list)
        {
            out << "Payload: " << format(message::payload_unreadable, {lic::to_string(list.error())}) << '\n';
            return shown;
        }
        print_revocations(out, *list);
        shown.payload_read = true;
        return shown;
    }
    print_signature(out, check_signature(file, file.purpose, keys, nullptr));
    out << "Payload: " << format(message::purpose_unknown, {file.purpose}) << '\n';
    return shown;
}

// Reads a signed file from disk: its text, and the envelope. The error is the sentence to print.
struct LoadedFile
{
    std::string name;
    std::string text;
    sf::Envelope envelope;
};

[[nodiscard]] std::expected<LoadedFile, std::string> load_envelope(const std::string& name)
{
    auto text = read_file(path_from_utf8(name), sf::max_file_size);
    if (!text)
    {
        return std::unexpected(format(message::cannot_read, {name, text.error()}));
    }
    auto envelope = sf::parse(*text);
    if (!envelope)
    {
        return std::unexpected(format(message::cannot_read, {name, lic::to_string(envelope.error())}));
    }
    return LoadedFile{.name = name, .text = std::move(*text), .envelope = std::move(*envelope)};
}

// What `issue` and `revocations` check before they sign: the options that name the files and the key.
// Returns a usage problem, if there is one.
[[nodiscard]] std::optional<std::string> signing_options_problem(std::string_view command, const Options& options)
{
    if (!options.has("--request"))
    {
        return format(message::needs_option, {command, "--request"});
    }
    if (options.has("--dry-run"))
    {
        return std::nullopt;
    }
    if (!options.has("--out"))
    {
        return format(message::needs_option, {command, "--out"});
    }
    if (options.has("--test"))
    {
        if (options.has("--key-id") || options.has("--key-file"))
        {
            return std::string(message::test_or_key);
        }
        return std::nullopt;
    }
    if (!options.has("--key-id") || !options.has("--key-file"))
    {
        return format(message::needs_key_pair, {command});
    }
    const std::string id = options.value("--key-id");
    if (!is_key_id(id))
    {
        return format(message::bad_key_id, {id});
    }
    if (id.starts_with(sf::test_key_prefix))
    {
        return format(message::key_not_for_test, {id});
    }
    return std::nullopt;
}

// What a successful issue or revocations reports.
void print_written(std::ostream& out, const std::string& out_name, const WrittenFile& written,
                   const SigningKey& key, std::string_view purpose)
{
    sf::TrustedKey own;
    own.key_id = key.id();
    own.purpose = std::string(purpose);
    own.key = lic::signing::public_key_of(key.seed());
    out << format(message::wrote_file, {out_name}) << '\n';
    out << "Bytes: " << written.bytes << '\n';
    out << "Key: " << key.id() << '\n';
    if (key.is_test())
    {
        out << "Test key: yes\n";
    }
    out << "Signature: " << message::signature_verifies << '\n';
    out << "Key trusted by this build: " << (trusted_by_this_build(own) ? "yes" : "no") << '\n';
}

} // namespace

int fail(const Console& console, std::string_view text)
{
    console.err << "adl_licence_owner: " << text << '\n';
    return exit_failed;
}

int usage_error(const Console& console, std::string_view text)
{
    console.err << "adl_licence_owner: " << text << '\n' << message::help_hint << '\n';
    return exit_usage;
}

bool is_licence_number(std::string_view text) noexcept
{
    // ADL-2026-000123
    constexpr std::string_view prefix = lic::licence_id_prefix;
    if (!text.starts_with(prefix) || text.size() != prefix.size() + 4 + 1 + 6)
    {
        return false;
    }
    text.remove_prefix(prefix.size());
    const auto digit = [](char c) { return c >= '0' && c <= '9'; };
    return std::ranges::all_of(text.substr(0, 4), digit) && text[4] == '-' &&
           std::ranges::all_of(text.substr(5), digit) && text.front() >= '2';
}

bool is_test_licence_number(std::string_view text) noexcept
{
    return is_licence_number(text) && text[text.size() - 6] == '9';
}

int inspect_command(std::span<const std::string> arguments, const Console& console)
{
    static constexpr std::array<OptionSpec, 2> specs{{{"--test", false, false}, {"--trust", true, true}}};
    const auto options = parse_options("inspect", arguments, specs, 1);
    if (!options)
    {
        return usage_error(console, options.error());
    }
    if (options->positional().size() != 1)
    {
        return usage_error(console, format(message::needs_one_file, {"inspect"}));
    }
    say_test_note(console, *options);
    const auto keys = collect_keys(*options);
    if (!keys)
    {
        return fail(console, keys.error());
    }
    const auto file = load_envelope(options->positional().front());
    if (!file)
    {
        return fail(console, file.error());
    }
    return show_signed_file(console.out, file->envelope, *keys).payload_read ? exit_ok : exit_failed;
}

int verify_command(std::span<const std::string> arguments, const Console& console)
{
    static constexpr std::array<OptionSpec, 7> specs{{{"--test", false, false},
                                                      {"--trust", true, true},
                                                      {"--now", true, false},
                                                      {"--computer", true, false},
                                                      {"--latest-seen", true, false},
                                                      {"--build-time", true, false},
                                                      {"--revocations", true, false}}};
    const auto options = parse_options("verify", arguments, specs, 1);
    if (!options)
    {
        return usage_error(console, options.error());
    }
    if (options->positional().size() != 1)
    {
        return usage_error(console, format(message::needs_one_file, {"verify"}));
    }
    std::array<std::optional<lic::TimePoint>, 3> times{};
    constexpr std::array<std::string_view, 3> time_names{"--now", "--latest-seen", "--build-time"};
    for (std::size_t i = 0; i < times.size(); ++i)
    {
        const auto given = time_option(*options, time_names[i]);
        if (!given)
        {
            return usage_error(console, given.error());
        }
        times[i] = *given;
    }
    say_test_note(console, *options);
    const auto keys = collect_keys(*options);
    if (!keys)
    {
        return fail(console, keys.error());
    }
    const std::string name = options->positional().front();
    const auto text = read_file(path_from_utf8(name), sf::max_file_size);
    if (!text)
    {
        return fail(console, format(message::cannot_read, {name, text.error()}));
    }

    // A revocation list is checked and read; anything else is judged as a licence file.
    if (const auto envelope = sf::parse(*text); envelope && envelope->purpose == lic::revocations_purpose)
    {
        const Shown shown = show_signed_file(console.out, *envelope, *keys);
        return shown.payload_read && shown.signature_verifies ? exit_ok : exit_failed;
    }

    lic::RevocationList revocations = lic::builtin_revocations();
    if (options->has("--revocations"))
    {
        const std::string list_name = options->value("--revocations");
        const auto list_text = read_file(path_from_utf8(list_name), sf::max_file_size);
        if (!list_text)
        {
            return fail(console, format(message::cannot_read, {list_name, list_text.error()}));
        }
        const auto list = lic::read_revocations(*list_text, *keys);
        if (!list)
        {
            return fail(console, format(message::revocations_refused, {list_name, lic::to_string(list.error())}));
        }
        revocations = lic::newer(revocations, *list);
    }

    lic::Context where;
    where.now = times[0] ? *times[0] : std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    where.latest_seen = times[1] ? *times[1] : lic::TimePoint::min();
    where.build_time = times[2] ? *times[2] : lic::TimePoint::min();
    where.computer_name = options->value("--computer");
    where.keys = *keys;
    where.revocations = &revocations;

    const lic::Status status = lic::evaluate(*text, where);
    std::ostream& out = console.out;
    out << "Purpose: " << lic::licence_purpose << '\n';
    if (!status.key_id.empty())
    {
        out << "Key: " << status.key_id << '\n';
        if (status.key_id.starts_with(sf::test_key_prefix))
        {
            out << "Test key: yes\n";
        }
        out << "Signature: " << message::signature_verifies << '\n';
        const auto signer = std::ranges::find_if(*keys,
                                                 [&](const sf::TrustedKey& key)
                                                 { return key.key_id == status.key_id && key.purpose == lic::licence_purpose; });
        if (signer != keys->end())
        {
            out << "Key trusted by this build: " << (trusted_by_this_build(*signer) ? "yes" : "no") << '\n';
        }
    }
    out << "State: " << lic::state_name(status.state) << '\n';
    out << "Reason: " << reason_name(status.reason) << '\n';
    if (status.reason != lic::Reason::ok)
    {
        out << "Why: " << lic::reason_sentence(status.reason) << '\n';
    }
    if (status.active())
    {
        out << "Days left: " << status.days_left << '\n';
    }
    out << "Edition: " << lic::edition_name(status) << '\n';
    if (status.value)
    {
        print_licence(out, *status.value);
    }
    return status.active() ? exit_ok : exit_failed;
}

int issue_command(std::span<const std::string> arguments, const Console& console)
{
    static constexpr std::array<OptionSpec, 6> specs{{{"--request", true, false},
                                                      {"--out", true, false},
                                                      {"--key-id", true, false},
                                                      {"--key-file", true, false},
                                                      {"--test", false, false},
                                                      {"--dry-run", false, false}}};
    const auto options = parse_options("issue", arguments, specs, 0);
    if (!options)
    {
        return usage_error(console, options.error());
    }
    if (const auto problem = signing_options_problem("issue", *options))
    {
        return usage_error(console, *problem);
    }
    say_test_note(console, *options);

    const std::string request_name = options->value("--request");
    const auto request = read_file(path_from_utf8(request_name), sf::max_file_size);
    if (!request)
    {
        return fail(console, format(message::cannot_read, {request_name, request.error()}));
    }
    const auto value = lic::parse_payload(as_bytes(*request));
    if (!value)
    {
        return fail(console, format(message::request_refused,
                                    {request_name, format(message::request_not_read, {lic::to_string(value.error())})}));
    }
    const bool test = options->has("--test");
    if (!test && value->product != lic::product_id)
    {
        return fail(console, format(message::request_refused, {request_name, format(message::product_wrong, {value->product})}));
    }
    if (!is_licence_number(value->licence_id))
    {
        return fail(console, format(message::request_refused, {request_name, format(message::id_wrong, {value->licence_id})}));
    }
    if (!test && is_test_licence_number(value->licence_id))
    {
        return fail(console,
                    format(message::request_refused, {request_name, format(message::id_in_test_range, {value->licence_id})}));
    }
    const auto payload = lic::write_payload(*value);
    if (!payload)
    {
        return fail(console, format(message::cannot_sign, {lic::to_string(payload.error())}));
    }

    if (options->has("--dry-run"))
    {
        print_licence(console.out, *value);
        console.out << message::dry_run_done << '\n';
        return exit_ok;
    }

    SigningKey key;
    if (const auto loaded = key.load(*options); !loaded)
    {
        return fail(console, loaded.error());
    }
    const std::string out_name = options->value("--out");
    const auto written = sign_and_write(
        lic::licence_purpose, key, *payload, path_from_utf8(out_name),
        [&value](const std::string& read_back) -> std::optional<std::string>
        {
            const auto again = lic::parse_payload(as_bytes(read_back));
            if (!again || *again != *value)
            {
                return std::string(message::read_back_differs);
            }
            return std::nullopt;
        });
    if (!written)
    {
        return fail(console, written.error());
    }
    print_written(console.out, out_name, *written, key, lic::licence_purpose);
    print_licence(console.out, *value);
    return exit_ok;
}

int revocations_command(std::span<const std::string> arguments, const Console& console)
{
    static constexpr std::array<OptionSpec, 6> specs{{{"--request", true, false},
                                                      {"--out", true, false},
                                                      {"--key-id", true, false},
                                                      {"--key-file", true, false},
                                                      {"--test", false, false},
                                                      {"--dry-run", false, false}}};
    const auto options = parse_options("revocations", arguments, specs, 0);
    if (!options)
    {
        return usage_error(console, options.error());
    }
    if (const auto problem = signing_options_problem("revocations", *options))
    {
        return usage_error(console, *problem);
    }
    say_test_note(console, *options);

    const std::string request_name = options->value("--request");
    const auto request = read_file(path_from_utf8(request_name), sf::max_file_size);
    if (!request)
    {
        return fail(console, format(message::cannot_read, {request_name, request.error()}));
    }
    const auto value = lic::parse_revocations(as_bytes(*request));
    if (!value)
    {
        return fail(console, format(message::request_refused,
                                    {request_name, format(message::request_not_read, {lic::to_string(value.error())})}));
    }
    if (value->sequence == 0)
    {
        return fail(console, format(message::request_refused, {request_name, message::sequence_zero}));
    }
    const auto payload = lic::write_revocations(*value);
    if (!payload)
    {
        return fail(console, format(message::cannot_sign, {lic::to_string(payload.error())}));
    }

    if (options->has("--dry-run"))
    {
        print_revocations(console.out, *value);
        console.out << message::dry_run_done << '\n';
        return exit_ok;
    }

    SigningKey key;
    if (const auto loaded = key.load(*options); !loaded)
    {
        return fail(console, loaded.error());
    }
    const std::string out_name = options->value("--out");
    const auto written = sign_and_write(
        lic::revocations_purpose, key, *payload, path_from_utf8(out_name),
        [&value](const std::string& read_back) -> std::optional<std::string>
        {
            const auto again = lic::parse_revocations(as_bytes(read_back));
            if (!again || *again != *value)
            {
                return std::string(message::read_back_differs);
            }
            return std::nullopt;
        });
    if (!written)
    {
        return fail(console, written.error());
    }
    print_written(console.out, out_name, *written, key, lic::revocations_purpose);
    print_revocations(console.out, *value);
    return exit_ok;
}

int keygen_command(std::span<const std::string> arguments, const Console& console)
{
    static constexpr std::array<OptionSpec, 3> specs{
        {{"--key-id", true, false}, {"--purpose", true, false}, {"--out", true, false}}};
    const auto options = parse_options("keygen", arguments, specs, 0);
    if (!options)
    {
        return usage_error(console, options.error());
    }
    for (const std::string_view name : {"--key-id", "--purpose", "--out"})
    {
        if (!options->has(name))
        {
            return usage_error(console, format(message::needs_option, {"keygen", name}));
        }
    }
    const std::string id = options->value("--key-id");
    const std::string purpose = options->value("--purpose");
    if (!is_key_id(id))
    {
        return usage_error(console, format(message::bad_key_id, {id}));
    }
    if (id.starts_with(sf::test_key_prefix))
    {
        return usage_error(console, format(message::key_not_for_test, {id}));
    }
    if (purpose != lic::licence_purpose && purpose != lic::revocations_purpose)
    {
        return usage_error(console, format(message::bad_purpose, {purpose}));
    }

    const std::string folder_name = options->value("--out");
    const fs::path folder = path_from_utf8(folder_name);
    std::error_code error;
    if (!fs::is_directory(folder, error))
    {
        return fail(console, format(message::folder_missing, {folder_name}));
    }
    if (inside_git_work_tree(folder))
    {
        return fail(console, format(message::folder_in_repository, {folder_name}));
    }
    const fs::path private_path = folder / path_from_utf8(id + ".adlkey");
    const fs::path public_path = folder / path_from_utf8(id + ".adlpub");
    for (const fs::path& path : {private_path, public_path})
    {
        if (fs::exists(fs::symlink_status(path, error)))
        {
            return fail(console, format(message::cannot_write, {path_to_utf8(path), message::reason_exists}));
        }
    }

    // 32 random bytes from the system's source of them. The key is refused if it came out as the test key
    // or as nothing but zeros, which would mean the source is not working.
    lic::signing::Seed seed{};
    try
    {
        std::random_device device;
        for (std::size_t i = 0; i < seed.size(); i += 4)
        {
            const std::uint32_t word = device();
            for (std::size_t k = 0; k < 4; ++k)
            {
                seed[i + k] = static_cast<std::uint8_t>((word >> (8 * k)) & 0xFF);
            }
        }
    }
    catch (const std::exception&)
    {
        lic::signing::wipe(seed);
        return fail(console, message::no_random);
    }
    if (std::ranges::all_of(seed, [](std::uint8_t b) { return b == 0; }) || seed == lic::test_keys::private_key())
    {
        lic::signing::wipe(seed);
        return fail(console, message::random_refused);
    }

    sf::TrustedKey pub;
    pub.key_id = id;
    pub.purpose = purpose;
    pub.key = lic::signing::public_key_of(seed);

    std::string secret = lic::signing::format_seed(seed);
    secret.push_back('\n');
    const auto remove_both = [&]
    {
        remove_file(private_path);
        remove_file(public_path);
    };
    const auto stop = [&](std::string_view why)
    {
        remove_both();
        lic::signing::wipe(seed);
        lic::signing::wipe(secret);
        return fail(console, format(message::key_removed, {why}));
    };

    if (const auto written = write_new_file(private_path, secret, true); !written)
    {
        lic::signing::wipe(seed);
        lic::signing::wipe(secret);
        return fail(console, format(message::cannot_write, {path_to_utf8(private_path), written.error()}));
    }
    lic::signing::wipe(secret);
    if (const auto written = write_new_file(public_path, format_public_key(pub), false); !written)
    {
        return stop(format(message::cannot_write, {path_to_utf8(public_path), written.error()}));
    }

    // Read both back: the private file must give the same key, the public file the same public half.
    auto private_text = read_file(private_path, max_public_key_file_size);
    std::optional<lic::signing::Seed> again;
    if (private_text)
    {
        const auto parsed = lic::signing::parse_seed(*private_text);
        lic::signing::wipe(*private_text);
        if (parsed)
        {
            again = *parsed;
        }
    }
    bool same = again && *again == seed;
    if (const auto public_text = read_file(public_path, max_public_key_file_size))
    {
        const auto public_again = parse_public_key(*public_text);
        same = same && public_again && public_again->key == pub.key && public_again->key_id == pub.key_id &&
               public_again->purpose == pub.purpose;
    }
    else
    {
        same = false;
    }
    if (again)
    {
        lic::signing::wipe(*again);
    }
    if (!same)
    {
        return stop(message::key_check_failed);
    }
    lic::signing::wipe(seed);

    std::ostream& out = console.out;
    out << format(message::key_written_private, {path_to_utf8(private_path)}) << '\n';
    out << message::key_private_advice1 << '\n' << message::key_private_advice2 << '\n';
    out << format(message::key_written_public, {path_to_utf8(public_path)}) << '\n';
    out << message::key_public_advice << '\n';
    out << "Key: " << id << '\n';
    out << "Purpose: " << purpose << '\n';
    out << "Public key: " << sf::base64url_encode(pub.key) << '\n';
    out << "Public key (hex): " << hex(pub.key) << '\n';
    out << message::key_trust_advice1 << '\n' << message::key_trust_advice2 << '\n';
    out << message::key_standby_advice << '\n';
    return exit_ok;
}

} // namespace adl::licence_owner
