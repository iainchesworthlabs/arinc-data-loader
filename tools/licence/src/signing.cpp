// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "signing.hpp"

#include "adl/licence/test_keys.hpp"
#include "files.hpp"
#include "messages.hpp"
#include "public_key.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>

namespace adl::licence_owner
{

namespace lic = adl::licence;
namespace sf = adl::licence::signed_file;

namespace
{

// A key file is 43 characters and a line end. Anything much larger is not one.
constexpr std::size_t max_key_file_size = 4096;

[[nodiscard]] std::span<const std::uint8_t> as_bytes(const std::string& text) noexcept
{
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

} // namespace

SigningKey::~SigningKey()
{
    lic::signing::wipe(seed_);
}

void SigningKey::set(std::string id, const lic::signing::Seed& seed)
{
    id_ = std::move(id);
    seed_ = seed;
}

std::expected<void, std::string> SigningKey::load(const Options& options)
{
    if (options.has("--test"))
    {
        id_ = std::string(lic::test_keys::key_id);
        seed_ = lic::test_keys::private_key();
        test_ = true;
        return {};
    }
    // The commands have checked the options (signing_options_problem in commands.cpp): both are given and the
    // key id is one that may be used.
    const std::string id = options.value("--key-id");
    const std::string file = options.value("--key-file");
    auto text = read_file(path_from_utf8(file), max_key_file_size);
    if (!text)
    {
        return std::unexpected(format(message::key_file_refused, {file, text.error()}));
    }
    const auto seed = lic::signing::parse_seed(*text);
    lic::signing::wipe(*text);
    if (!seed)
    {
        return std::unexpected(format(message::key_file_refused, {file, message::key_file_not_a_key}));
    }
    id_ = id;
    seed_ = *seed;
    test_ = false;
    return {};
}

std::expected<WrittenFile, std::string> sign_and_write(std::string_view purpose, const SigningKey& key,
                                                       const std::string& payload, const std::filesystem::path& out,
                                                       const PayloadCheck& check)
{
    const std::string out_name = path_to_utf8(out);
    const sf::Envelope envelope = lic::signing::sign(purpose, key.id(), as_bytes(payload), key.seed());
    const std::string text = sf::write(envelope);
    if (const auto written = write_new_file(out, text, false); !written)
    {
        return std::unexpected(format(message::cannot_write, {out_name, written.error()}));
    }

    const auto failed = [&](std::string_view why) -> std::expected<WrittenFile, std::string>
    {
        remove_file(out);
        return std::unexpected(format(message::read_back_failed, {out_name, why}));
    };

    const auto bytes = read_file(out, sf::max_file_size);
    if (!bytes)
    {
        return failed(bytes.error());
    }
    const auto parsed = sf::parse(*bytes);
    if (!parsed)
    {
        return failed(lic::to_string(parsed.error()));
    }
    sf::TrustedKey own;
    own.key_id = key.id();
    own.purpose = std::string(purpose);
    own.key = lic::signing::public_key_of(key.seed());
    const auto verified = sf::verify(*parsed, purpose, std::span<const sf::TrustedKey>(&own, 1));
    if (!verified)
    {
        return failed(lic::to_string(verified.error()));
    }
    const std::string read_payload(parsed->payload.begin(), parsed->payload.end());
    if (read_payload != payload)
    {
        return failed(message::read_back_differs);
    }
    if (const auto why = check(read_payload))
    {
        return failed(*why);
    }
    return WrittenFile{.bytes = bytes->size()};
}

} // namespace adl::licence_owner
