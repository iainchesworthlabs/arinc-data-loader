// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// The signature check behind licence files: the Ed25519 code against the vectors of RFC 8032 and Project
// Wycheproof, and the signed envelope.

#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/test_keys.hpp"
#include "json.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <monocypher-ed25519.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace adl::licence;
using namespace adl::test;
namespace sf = adl::licence::signed_file;

namespace
{

template <std::size_t N>
std::array<std::uint8_t, N> hex_array(std::string_view text)
{
    const auto bytes = hex_bytes(text);
    REQUIRE(bytes.size() == N);
    std::array<std::uint8_t, N> out{};
    std::ranges::copy(bytes, out.begin());
    return out;
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct RfcVector
{
    std::string_view seed;
    std::string_view public_key;
    std::string_view message;
    std::string_view signature;
};

// RFC 8032, section 7.1, tests 1 to 3.
constexpr std::array<RfcVector, 3> rfc_vectors{{
    {"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
     "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
     "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
     "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
    {"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
     "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
     "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
     "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
    {"c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
     "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
     "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac"
     "18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"},
}};

sf::Envelope signed_envelope(std::string_view purpose, std::string_view payload)
{
    return signing::sign(purpose, test_keys::key_id, span_of(payload), test_keys::private_key());
}

} // namespace

TEST_CASE("licence signature: RFC 8032 vectors", "[licence][signature]")
{
    for (const RfcVector& vector : rfc_vectors)
    {
        auto seed = hex_array<32>(vector.seed);
        const auto public_key = hex_array<32>(vector.public_key);
        const auto message = hex_bytes(vector.message);
        const auto expected = hex_array<64>(vector.signature);

        REQUIRE(signing::public_key_of(seed) == public_key);

        std::array<std::uint8_t, 64> secret{};
        std::array<std::uint8_t, 32> derived{};
        crypto_ed25519_key_pair(secret.data(), derived.data(), seed.data());
        std::array<std::uint8_t, 64> made{};
        crypto_ed25519_sign(made.data(), secret.data(), message.data(), message.size());
        REQUIRE(made == expected);
        REQUIRE(crypto_ed25519_check(expected.data(), public_key.data(), message.data(), message.size()) == 0);
        REQUIRE(sf::verify_ed25519(expected, public_key, message));

        auto wrong = expected;
        wrong[10] ^= 0x01U;
        REQUIRE(crypto_ed25519_check(wrong.data(), public_key.data(), message.data(), message.size()) != 0);
        REQUIRE(!sf::verify_ed25519(wrong, public_key, message));
    }
}

TEST_CASE("licence signature: the test key is the key of RFC 8032 test 1", "[licence][signature]")
{
    REQUIRE(test_keys::private_key() == hex_array<32>(rfc_vectors[0].seed));
    REQUIRE(test_keys::trusted()[0].key == hex_array<32>(rfc_vectors[0].public_key));
    REQUIRE(signing::public_key_of(test_keys::private_key()) == test_keys::trusted()[0].key);
}

TEST_CASE("licence signature: Wycheproof Ed25519 vectors", "[licence][signature]")
{
    const auto text = read_file(std::filesystem::path(ADL_LICENCE_TEST_DATA_DIR) / "wycheproof" / "ed25519_test.json");
    const auto document = detail::parse_strict(text);
    REQUIRE(document);
    const detail::JsonObject* root = document->as_object();
    REQUIRE(root != nullptr);
    const detail::JsonValue* groups = detail::find(*root, "testGroups");
    REQUIRE(groups != nullptr);
    REQUIRE(groups->as_array() != nullptr);

    int valid = 0;
    int invalid = 0;
    std::string wrong;
    for (const detail::JsonValue& group_value : *groups->as_array())
    {
        const detail::JsonObject* group = group_value.as_object();
        REQUIRE(group != nullptr);
        const detail::JsonObject* key_object = detail::find(*group, "publicKey")->as_object();
        REQUIRE(key_object != nullptr);
        const auto public_key = hex_array<32>(*detail::find(*key_object, "pk")->as_string());
        for (const detail::JsonValue& test_value : *detail::find(*group, "tests")->as_array())
        {
            const detail::JsonObject& test = *test_value.as_object();
            const std::string& expected = *detail::find(test, "result")->as_string();
            const auto message = hex_bytes(*detail::find(test, "msg")->as_string());
            const auto sig = hex_bytes(*detail::find(test, "sig")->as_string());
            // The envelope refuses a signature that is not 64 bytes before any curve arithmetic, so such a
            // vector is rejected by size.
            bool accepted = false;
            if (sig.size() == 64)
            {
                sf::Signature signature{};
                std::ranges::copy(sig, signature.begin());
                accepted = sf::verify_ed25519(signature, public_key, message);
            }
            if (expected == "valid")
            {
                ++valid;
            }
            else if (expected == "invalid")
            {
                ++invalid;
            }
            // An "acceptable" vector may go either way.
            if ((expected == "valid" && !accepted) || (expected == "invalid" && accepted))
            {
                wrong += std::to_string(*detail::find(test, "tcId")->as_integer()) + " ";
            }
        }
    }
    // The test case ids of every vector that went the wrong way.
    REQUIRE(wrong.empty());
    // The file has to have been read: both kinds are there in number.
    REQUIRE(valid > 50);
    REQUIRE(invalid > 50);
}

TEST_CASE("licence signature: encodings RFC 8032 does not allow are refused", "[licence][signature]")
{
    // Wycheproof test 151: the first half of the signature is the point with y = 1 written with the sign bit
    // of x set. Monocypher alone accepts it; the RFC does not.
    const auto key = hex_array<32>(rfc_vectors[0].public_key);
    const auto message = hex_bytes("313233343030");
    const auto sig = hex_array<64>("0100000000000000000000000000000000000000000000000000000000000080"
                                   "c803ee1f2342aa96ff698a393d1ab5e66f3eda101d6d120b394c3fd32c117d0a");
    REQUIRE(crypto_ed25519_check(sig.data(), key.data(), message.data(), message.size()) == 0);
    REQUIRE(!sf::verify_ed25519(sig, key, message));

    // A y coordinate at or above the field prime, in the signature or in the key, never reaches the
    // arithmetic.
    const auto good_sig = hex_array<64>(rfc_vectors[0].signature);
    REQUIRE(sf::verify_ed25519(good_sig, key, {}));
    for (const std::string_view y : {"edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
                                     "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
                                     "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                                     "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"})
    {
        auto bad_sig = good_sig;
        std::ranges::copy(hex_array<32>(y), bad_sig.begin());
        REQUIRE(!sf::verify_ed25519(bad_sig, key, {}));
        REQUIRE(!sf::verify_ed25519(good_sig, hex_array<32>(y), {}));
    }
}

TEST_CASE("signed file: base64url round trip and strictness", "[licence][envelope]")
{
    for (std::size_t size = 0; size < 70; ++size)
    {
        std::vector<std::uint8_t> data(size);
        for (std::size_t i = 0; i < size; ++i)
        {
            data[i] = static_cast<std::uint8_t>(i * 37U + size);
        }
        const std::string text = sf::base64url_encode(data);
        REQUIRE(text.find('=') == std::string::npos);
        const auto back = sf::base64url_decode(text);
        REQUIRE(back);
        REQUIRE(*back == data);
    }

    REQUIRE(sf::base64url_encode(span_of("\xfb\xff")) == "-_8");
    REQUIRE(sf::base64url_decode(""));

    // Padding, white space, the other alphabet, an impossible length and spare bits that are not zero.
    for (const char* bad : {"QQ==", "QQ ", "Q\nQ", "+/8", "Q", "QUJDR", "QR", "QUF"})
    {
        const auto decoded = sf::base64url_decode(bad);
        REQUIRE(!decoded);
        REQUIRE(decoded.error() == Error::malformed);
    }
}

TEST_CASE("signed file: write and parse round trip", "[licence][envelope]")
{
    const sf::Envelope file = signed_envelope("licence", "{\"a\":1}");
    const std::string text = sf::write(file);
    const auto parsed = sf::parse(text);
    REQUIRE(parsed);
    REQUIRE(parsed->format == sf::current_format);
    REQUIRE(parsed->purpose == "licence");
    REQUIRE(parsed->key_id == test_keys::key_id);
    REQUIRE(parsed->payload == file.payload);
    REQUIRE(parsed->signature == file.signature);
}

TEST_CASE("signed file: parse refuses what it should", "[licence][envelope]")
{
    const std::string good = sf::write(signed_envelope("licence", "{}"));
    REQUIRE(sf::parse(good));

    const auto expect = [](std::string_view text, Error expected)
    {
        const auto parsed = sf::parse(text);
        REQUIRE(!parsed);
        REQUIRE(parsed.error() == expected);
    };
    const auto replaced = [&good](std::string_view from, std::string_view to)
    {
        std::string text = good;
        const auto position = text.find(from);
        REQUIRE(position != std::string::npos);
        text.replace(position, from.size(), to);
        return text;
    };

    expect(std::string(sf::max_file_size + 1, ' '), Error::too_large);
    expect("", Error::malformed);
    expect("not json", Error::malformed);
    expect("[]", Error::malformed);
    expect("{}", Error::malformed);
    expect(good + "x", Error::malformed);

    expect(replaced("\"format\":1", "\"format\":2"), Error::newer_version);
    expect(replaced("\"format\":1", "\"format\":0"), Error::malformed);
    expect(replaced("\"format\":1", "\"format\":1.0"), Error::malformed);
    expect(replaced("\"format\":1", "\"format\":\"1\""), Error::malformed);
    // A sixth field, and the same key twice.
    expect(replaced("\"format\":1", "\"format\":1,\"extra\":true"), Error::malformed);
    expect(replaced("\"format\":1", "\"format\":1,\"format\":1"), Error::malformed);
    expect(replaced("\"purpose\":\"licence\"", "\"purpose\":\"Licence\""), Error::malformed);
    expect(replaced("\"purpose\":\"licence\"", "\"purpose\":7"), Error::malformed);
    expect(replaced("\"key_id\":\"test-", "\"key_id\":\"test "), Error::malformed);
    // A signature one character short, and one that is not base64url.
    expect(replaced("\"signature\":\"", "\"signature\":\"="), Error::malformed);
    {
        std::string text = good;
        const auto position = text.find("\"signature\":\"") + 13;
        text.erase(position, 1);
        expect(text, Error::malformed);
    }
}

TEST_CASE("signed file: verify accepts a good file and names the key", "[licence][envelope]")
{
    const sf::Envelope file = signed_envelope("licence", "payload");
    const auto key = sf::verify(file, "licence", test_keys::trusted());
    REQUIRE(key);
    REQUIRE((*key)->key_id == test_keys::key_id);
    REQUIRE((*key)->purpose == "licence");
}

TEST_CASE("signed file: verify refuses a changed file", "[licence][envelope]")
{
    const sf::Envelope file = signed_envelope("licence", "payload");
    const auto expect = [](const sf::Envelope& changed, Error expected)
    {
        const auto key = sf::verify(changed, "licence", test_keys::trusted());
        REQUIRE(!key);
        REQUIRE(key.error() == expected);
    };

    sf::Envelope changed = file;
    changed.payload[0] ^= 0x01U;
    expect(changed, Error::bad_signature);

    changed = file;
    changed.payload.push_back(0);
    expect(changed, Error::bad_signature);

    changed = file;
    changed.signature[63] ^= 0x80U;
    expect(changed, Error::bad_signature);

    changed = file;
    changed.key_id = "lic-2026-a";
    expect(changed, Error::unknown_key);

    changed = file;
    changed.format = 2;
    expect(changed, Error::newer_version);

    const auto wrong = sf::verify(file, "revocations", test_keys::trusted());
    REQUIRE(!wrong);
    REQUIRE(wrong.error() == Error::wrong_purpose);

    const auto none = sf::verify(file, "licence", {});
    REQUIRE(!none);
    REQUIRE(none.error() == Error::unknown_key);
}

TEST_CASE("signed file: a signature for one purpose never passes as another", "[licence][envelope]")
{
    // The same key is trusted for both purposes here, which is the mistake the purpose inside the signed
    // message guards against.
    sf::Envelope file = signed_envelope("revocations", "payload");
    REQUIRE(sf::verify(file, "revocations", test_keys::trusted()));

    file.purpose = "licence";
    const auto key = sf::verify(file, "licence", test_keys::trusted());
    REQUIRE(!key);
    REQUIRE(key.error() == Error::bad_signature);

    // The message is the name of the format, 0, purpose, 0, payload, so a purpose cannot run into the
    // payload either.
    const auto a = sf::signed_message("ab", span_of("c"));
    const auto b = sf::signed_message("a", span_of("bc"));
    REQUIRE(a != b);
    const std::string_view tag = "ICL-SIGNED-FILE-1";
    REQUIRE(std::equal(tag.begin(), tag.end(), a.begin()));
    REQUIRE(a[tag.size()] == 0);
    REQUIRE(a.size() == tag.size() + 1 + 2 + 1 + 1);
}

TEST_CASE("signed file: a key is trusted for its own purpose only", "[licence][envelope]")
{
    std::vector<sf::TrustedKey> keys{test_keys::trusted()[0]};
    REQUIRE(keys[0].purpose == "licence");
    const sf::Envelope file = signed_envelope("revocations", "payload");
    const auto key = sf::verify(file, "revocations", keys);
    REQUIRE(!key);
    REQUIRE(key.error() == Error::unknown_key);
}

TEST_CASE("signed file: key windows and withdrawal", "[licence][envelope]")
{
    using namespace std::chrono;
    const sf::TimePoint start{sys_days{2026y / January / 1}};
    const sf::TimePoint end{sys_days{2027y / January / 1}};

    sf::TrustedKey key = test_keys::trusted()[0];
    key.not_before = start;
    key.not_after = end;
    REQUIRE(sf::check_key_window(key, start));
    REQUIRE(sf::check_key_window(key, end));
    REQUIRE(!sf::check_key_window(key, start - seconds{1}));
    REQUIRE(!sf::check_key_window(key, end + seconds{1}));

    // Leaked on 1 June: files signed before then keep working.
    const sf::TimePoint leak{sys_days{2026y / June / 1}};
    key.revoked_from = leak;
    REQUIRE(sf::check_key_window(key, leak - seconds{1}));
    REQUIRE(!sf::check_key_window(key, leak));
    const sf::Envelope file = signed_envelope("licence", "payload");
    std::vector<sf::TrustedKey> keys{key};
    REQUIRE(sf::verify(file, "licence", keys));

    // Withdrawn outright: nothing it signed is accepted.
    keys[0].revoked_from = sf::TimePoint::min();
    const auto refused = sf::verify(file, "licence", keys);
    REQUIRE(!refused);
    REQUIRE(refused.error() == Error::key_not_valid);
}

TEST_CASE("signed file: a product trusts no test key", "[licence][envelope]")
{
    // No built-in key is a test key, and the product's list is the built-in one.
    for (const sf::TrustedKey& key : sf::builtin_keys())
    {
        REQUIRE(!key.key_id.starts_with(sf::test_key_prefix));
    }
    REQUIRE(sf::product_trust_list().size() == sf::builtin_keys().size());

    std::vector<sf::TrustedKey> keys(test_keys::trusted().begin(), test_keys::trusted().end());
    sf::TrustedKey real = keys[0];
    real.key_id = "lic-2026-a";
    keys.push_back(real);
    const auto kept = sf::without_test_keys(keys);
    REQUIRE(kept.size() == 1);
    REQUIRE(kept[0].key_id == "lic-2026-a");
}

TEST_CASE("signed file: this release trusts the four public keys of ARINC Data Loader and no others",
          "[licence][envelope]")
{
    // The public halves made by `adl_licence_owner keygen` on 2026-11-21 and checked against their
    // `.adlownerpub` files: a signing key and a standby for each of the two purposes. The hex is written here
    // from the key files, not from trusted_keys.cpp, so a byte changed there fails this test.
    struct Expected
    {
        std::string_view key_id;
        std::string_view purpose;
        std::string_view public_key;
    };
    constexpr std::array<Expected, 4> expected{{
        {"lic-2026-a", "licence", "8e4da274de8dbfa62c64b64a0b6a369b1bf7f660fc00f6aa1b635ad180ea5355"},
        {"rev-2026-a", "revocations", "ace666578367051b5ec737908edea9b241e45a4031b49344a76a8ab20aa319f6"},
        {"lic-2026-b", "licence", "469ec39ad1f8900942dc996469bc9efff59511263fb9320b907027821db28c06"},
        {"rev-2026-b", "revocations", "0797724ffea1a523d64ab4c50fe1007b772ef29f6ba6510d34506681e28d1737"},
    }};

    const auto release = sf::product_trust_list();
    REQUIRE(release.size() == expected.size());
    REQUIRE(sf::builtin_keys().size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        INFO(expected[i].key_id);
        CHECK(release[i].key_id == expected[i].key_id);
        CHECK(release[i].purpose == expected[i].purpose);
        CHECK(release[i].key == hex_array<32>(expected[i].public_key));
        // In force from the start, with nothing withdrawn: a later release sets `not_after` or `revoked_from`.
        CHECK(release[i].not_before == sf::TimePoint::min());
        CHECK(release[i].not_after == sf::TimePoint::max());
        CHECK(!release[i].revoked_from.has_value());
    }

    // An id names one key, and a key is used for one purpose: no two entries share an id or the bytes.
    for (std::size_t i = 0; i < release.size(); ++i)
    {
        for (std::size_t j = i + 1; j < release.size(); ++j)
        {
            CHECK(release[i].key_id != release[j].key_id);
            CHECK(release[i].key != release[j].key);
        }
    }

    // A licence key is trusted for licences and a revocation key for lists, and not the other way about: the id of
    // a licence key in a file of the other purpose names no key.
    const std::string payload = "{\"any\":\"payload\"}";
    for (const sf::TrustedKey& key : release)
    {
        const std::string_view other = key.purpose == "licence" ? "revocations" : "licence";
        const sf::Envelope other_purpose = signing::sign(other, key.key_id, span_of(payload), test_keys::private_key());
        const auto refused = sf::verify(other_purpose, other, release);
        REQUIRE(!refused);
        CHECK(refused.error() == Error::unknown_key);
    }
}

TEST_CASE("signed file: a release does not know the test key, and a forgery under a real id is refused",
          "[licence][envelope]")
{
    const auto release = sf::product_trust_list();
    const std::string payload = "{\"any\":\"payload\"}";

    // The test key under its own id is unknown to a release, and a program reads an unknown key as a licence
    // made for a newer version (which is what a licence signed with a key of a later release is).
    const sf::Envelope own = signing::sign("licence", test_keys::key_id, span_of(payload), test_keys::private_key());
    const auto unknown = sf::verify(own, "licence", release);
    REQUIRE(!unknown);
    CHECK(unknown.error() == Error::unknown_key);

    // The same file verifies where the test key is trusted, so the refusal above is the trust list and not
    // a broken file.
    REQUIRE(sf::verify(own, "licence", test_keys::trusted()));

    // The test key's signature under the id of a key the release does trust is a forgery: its key is known,
    // and its signature does not verify. Its private half is public (RFC 8032), so this is what anyone could
    // make.
    for (const sf::TrustedKey& key : release)
    {
        INFO(key.key_id);
        const sf::Envelope forged = signing::sign(key.purpose, key.key_id, span_of(payload), test_keys::private_key());
        const auto refused = sf::verify(forged, key.purpose, release);
        REQUIRE(!refused);
        CHECK(refused.error() == Error::bad_signature);
    }
}

TEST_CASE("licence errors: every code has its own message", "[licence][envelope]")
{
    std::vector<std::string_view> seen;
    for (const Error code : {Error::too_large, Error::malformed, Error::newer_version, Error::wrong_purpose,
                             Error::unknown_key, Error::key_not_valid, Error::bad_signature})
    {
        const std::string_view message = to_string(code);
        REQUIRE(!message.empty());
        REQUIRE(std::ranges::find(seen, message) == seen.end());
        seen.push_back(message);
    }
    REQUIRE(to_string(static_cast<Error>(99)) == "unknown licence error");
}

TEST_CASE("signed file: a private key has one text form", "[licence][envelope]")
{
    const signing::Seed key = test_keys::private_key();
    std::string text = signing::format_seed(key);
    REQUIRE(text.size() == 43);

    const auto plain = signing::parse_seed(text);
    REQUIRE(plain);
    REQUIRE(*plain == key);
    // A key file ends with a new line, and may have been saved on Windows.
    const auto spaced = signing::parse_seed("  " + text + "\r\n");
    REQUIRE(spaced);
    REQUIRE(*spaced == key);

    REQUIRE(!signing::parse_seed(""));
    REQUIRE(!signing::parse_seed(" \n"));
    REQUIRE(!signing::parse_seed("not a key"));
    // Well-formed base64url of the wrong length.
    REQUIRE(!signing::parse_seed(text.substr(0, 40)));
    REQUIRE(!signing::parse_seed(text + "AAAA"));

    signing::Seed copy = key;
    signing::wipe(copy);
    REQUIRE(copy == signing::Seed{});
    signing::wipe(text);
    REQUIRE(text.empty());
}
