// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// Licence payloads, the state a licence is in and what it grants.

#include "adl/licence/licence.hpp"
#include "adl/licence/product.hpp"
#include "adl/licence/sign.hpp"
#include "adl/licence/test_keys.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace adl::licence;
using namespace adl::test;
using std::chrono::minutes;
using std::chrono::seconds;
namespace sf = adl::licence::signed_file;

TEST_CASE("licence: times are read and written in one form", "[licence][payload]")
{
    const auto parsed = parse_time("2026-11-13T02:10:05Z");
    REQUIRE(parsed);
    REQUIRE(*parsed == at(2026, 11, 13, 2) + minutes{10} + seconds{5});
    REQUIRE(format_time(*parsed) == "2026-11-13T02:10:05Z");
    REQUIRE(parse_time("2028-02-29T23:59:59Z"));

    for (const char* bad : {"", "2026-11-13", "2026-11-13T02:10:05", "2026-11-13T02:10:05+00", "2026-11-13 02:10:05Z",
                            "2026-13-01T00:00:00Z", "2027-02-29T00:00:00Z", "2026-11-13T24:00:00Z",
                            "2026-11-13T00:60:00Z", "2026-11-13T00:00:60Z", "1999-12-31T00:00:00Z",
                            "2026-11-13T02:10:05z", "2026-1 -13T02:10:05Z", "2026-11-13T02:10:05Z "})
    {
        REQUIRE(!parse_time(bad));
    }
}

TEST_CASE("licence: payload round trip", "[licence][payload]")
{
    const Licence value = full();
    const auto text = write_payload(value);
    REQUIRE(text);
    const auto parsed = parse_payload(span_of(*text));
    REQUIRE(parsed);
    REQUIRE(*parsed == value);

    Licence plain = annual();
    plain.licensee_contact.clear();
    const auto plain_text = write_payload(plain);
    REQUIRE(plain_text);
    REQUIRE(plain_text->find("contact") == std::string::npos);
    REQUIRE(plain_text->find("updates_until") == std::string::npos);
    REQUIRE(plain_text->find("email") == std::string::npos);
    REQUIRE(*parse_payload(span_of(*plain_text)) == plain);
}

TEST_CASE("licence: a licence number is any upper-case token, and its shape is not checked", "[licence][payload]")
{
    // The owner's tool writes ADL-<year>-<six digits>. The reader takes what the signature covers: any upper-case
    // letters, digits and hyphens, so that the library can be shared by products with other prefixes.
    for (const std::string_view id : {"ADL-2026-000123", "VSS-2026-000123", "ADL-26-1", "X", "A-B-C-9"})
    {
        CAPTURE(id);
        auto value = annual();
        value.licence_id = std::string{id};
        const auto written = write_payload(value);
        REQUIRE(written);
        const auto read = parse_payload(span_of(*written));
        REQUIRE(read);
        REQUIRE(read->licence_id == id);
    }
    for (const std::string& id : {std::string{}, std::string{"adl-2026-000123"}, std::string{"ADL 2026"},
                                  std::string{"ADL_2026"}, std::string(65, 'A')})
    {
        CAPTURE(id);
        auto value = annual();
        value.licence_id = id;
        REQUIRE_FALSE(write_payload(value));
    }
}

TEST_CASE("licence: the documented payload is read", "[licence][payload]")
{
    const std::string_view payload = R"({
      "product": "adl",
      "licence_id": "ADL-2026-000123",
      "kind": "annual",
      "edition": "pro",
      "licensee": { "name": "Example Lab Pty Ltd", "contact": "Test team" },
      "issued_at": "2026-11-13T02:10:00Z",
      "starts_at": "2026-11-13T00:00:00Z",
      "expires_at": "2027-11-13T23:59:59Z",
      "grace_days": 28,
      "seats": 3,
      "features": ["support", "data_packs"],
      "machines": [],
      "updates_until": null,
      "replaces": null,
      "critical": []
    })";
    const auto parsed = parse_payload(span_of(payload));
    REQUIRE(parsed);
    REQUIRE(parsed->product == "adl");
    REQUIRE(parsed->licence_id == "ADL-2026-000123");
    REQUIRE(parsed->kind == LicenceKind::annual);
    REQUIRE(parsed->edition == Edition::pro);
    REQUIRE(parsed->licensee_name == "Example Lab Pty Ltd");
    REQUIRE(parsed->seats == 3);
    REQUIRE(parsed->grace_days == 28);
    REQUIRE(parsed->features.size() == 2);
    REQUIRE(parsed->machines.empty());
    REQUIRE(!parsed->updates_until);
    REQUIRE(parsed->replaces.empty());
}

TEST_CASE("licence: a payload this version cannot honour is refused", "[licence][payload]")
{
    REQUIRE(parse_payload(span_of(payload_with("\"seats\":3", "\"seats\":3"))));

    expect_payload_error("", Error::malformed);
    expect_payload_error("[]", Error::malformed);
    expect_payload_error("{}", Error::malformed);
    // The same key twice, wherever it is.
    expect_payload_error(payload_with("\"seats\":3", "\"seats\":3,\"seats\":900"), Error::malformed);
    expect_payload_error(payload_with("\"name\":", "\"name\":\"Someone Else\",\"name\":"), Error::malformed);

    expect_payload_error(payload_with("ADL-2026-000123", "adl 1"), Error::malformed);
    expect_payload_error(payload_with("\"adl\"", "\"ADL\""), Error::malformed);
    expect_payload_error(payload_with("\"product\":\"adl\"", "\"product\":7"), Error::malformed);
    expect_payload_error(payload_with("\"annual\"", "\"perpetual\""), Error::malformed);
    expect_payload_error(payload_with("\"pro\"", "\"community\""), Error::malformed);
    expect_payload_error(payload_with("Example Lab Pty Ltd", ""), Error::malformed);
    expect_payload_error(payload_with("Example Lab Pty Ltd", "Line\\nbreak"), Error::malformed);
    expect_payload_error(payload_with("\"seats\":3", "\"seats\":-1"), Error::malformed);
    expect_payload_error(payload_with("\"seats\":3", "\"seats\":3.5"), Error::malformed);
    expect_payload_error(payload_with("\"seats\":3", "\"seats\":1e2"), Error::malformed);
    expect_payload_error(payload_with("\"seats\":3", "\"seats\":99999999999999999999"), Error::malformed);
    expect_payload_error(payload_with("\"seats\":3", "\"seats\":\"3\""), Error::malformed);
    expect_payload_error(payload_with("\"grace_days\":28", "\"grace_days\":4000"), Error::malformed);
    expect_payload_error(payload_with("\"support\"", "\"Support Line\""), Error::malformed);
    expect_payload_error(payload_with("\"support\"", "\".support\""), Error::malformed);
    expect_payload_error(payload_with("\"support\"", "\"support.\""), Error::malformed);
    expect_payload_error(payload_with("\"support\"", "7"), Error::malformed);
    expect_payload_error(payload_with("\"machines\":[]", "\"machines\":[\"*\"]"), Error::malformed);
    expect_payload_error(payload_with("\"machines\":[]", "\"machines\":[\"rig 7\"]"), Error::malformed);
    expect_payload_error(payload_with("\"machines\":[]", "\"machines\":{}"), Error::malformed);
    // Ends before it starts.
    expect_payload_error(payload_with("2027-11-13T00:00:00Z", "2026-11-12T00:00:00Z"), Error::malformed);
    expect_payload_error(payload_with("2027-11-13T00:00:00Z", "next year"), Error::malformed);
    expect_payload_error(payload_with("\"licensee\":{", "\"licensee\":[{").append("]"), Error::malformed);
    expect_payload_error(payload_with("\"licensee\":{", "\"licensee\":7,\"x\":{"), Error::malformed);
    expect_payload_error(payload_with("\"critical\":[]", "\"critical\":[],\"updates_until\":\"soon\""),
                         Error::malformed);
    expect_payload_error(payload_with("\"critical\":[]", "\"critical\":[],\"replaces\":\"lower\""), Error::malformed);
    expect_payload_error(payload_with("\"critical\":[]", "\"critical\":[],\"clock_reset\":\"UPPER\""),
                         Error::malformed);
}

TEST_CASE("licence: unknown fields are ignored unless they are critical", "[licence][payload]")
{
    const auto ignored = parse_payload(span_of(payload_with("\"seats\":3", "\"seats\":3,\"colour\":\"teal\"")));
    REQUIRE(ignored);
    REQUIRE(*ignored == annual());

    // A critical field this version knows is fine.
    REQUIRE(parse_payload(span_of(payload_with("\"critical\":[]", "\"critical\":[\"machines\"]"))));

    expect_payload_error(payload_with("\"critical\":[]", "\"critical\":[\"max_ports\"],\"max_ports\":4"),
                         Error::newer_version);
    expect_payload_error(payload_with("\"critical\":[]", "\"critical\":7"), Error::malformed);
}

TEST_CASE("licence: states across the term", "[licence][state]")
{
    const std::string text = signed_text(annual());
    struct Row
    {
        TimePoint now;
        State expected_state;
        Reason expected_reason;
        int days_left;
        bool active;
    };
    const Row rows[] = {
        // Before the start. Issued at 02:00, so the day before is inside the clock tolerance.
        {at(2026, 11, 12, 12), State::not_yet_valid, Reason::not_yet_valid, 0, false},
        {at(2026, 11, 13), State::valid, Reason::ok, 365, true},
        {at(2027, 6, 1), State::valid, Reason::ok, 165, true},
        // 31 days left is valid, 30 is expiring.
        {at(2027, 10, 13), State::valid, Reason::ok, 31, true},
        {at(2027, 10, 14), State::expiring, Reason::ok, 30, true},
        {at(2027, 11, 12, 12), State::expiring, Reason::ok, 0, true},
        // The last second of the term.
        {at(2027, 11, 13), State::expiring, Reason::ok, 0, true},
        {at(2027, 11, 13) + seconds{1}, State::grace, Reason::ok, 27, true},
        {at(2027, 12, 1), State::grace, Reason::ok, 10, true},
        // The last second of the grace period.
        {at(2027, 12, 11), State::grace, Reason::ok, 0, true},
        {at(2027, 12, 11) + seconds{1}, State::expired, Reason::expired, 0, false},
        {at(2030, 1, 1), State::expired, Reason::expired, 0, false},
    };
    for (const Row& expected : rows)
    {
        const Status result = evaluate(text, on(expected.now));
        REQUIRE(result.state == expected.expected_state);
        REQUIRE(result.reason == expected.expected_reason);
        REQUIRE(result.days_left == expected.days_left);
        REQUIRE(result.active() == expected.active);
        REQUIRE(result.value);
        REQUIRE(*result.value == annual());
        REQUIRE(result.key_id == test_keys::key_id);
        REQUIRE(result.grace_ends == at(2027, 12, 11));
    }
}

TEST_CASE("licence: a trial has no grace period and a short warning", "[licence][state]")
{
    Licence trial = annual();
    trial.kind = LicenceKind::trial;
    trial.expires_at = at(2026, 11, 27);
    trial.grace_days = 0;
    trial.seats = 1;
    const std::string text = signed_text(trial);

    REQUIRE(evaluate(text, on(at(2026, 11, 20))).state == State::valid);
    REQUIRE(evaluate(text, on(at(2026, 11, 23))).state == State::valid);
    const Status last_days = evaluate(text, on(at(2026, 11, 24)));
    REQUIRE(last_days.state == State::expiring);
    REQUIRE(last_days.days_left == 3);
    const Status over = evaluate(text, on(at(2026, 11, 27) + seconds{1}));
    REQUIRE(over.state == State::expired);
    REQUIRE(!over.active());
}

TEST_CASE("licence: files that are not valid licences", "[licence][state]")
{
    const Context where = on(at(2027, 1, 1));
    const auto expect = [&where](std::string_view text, Reason expected)
    {
        const Status result = evaluate(text, where);
        REQUIRE(result.state == State::invalid);
        REQUIRE(result.reason == expected);
        REQUIRE(!result.active());
        REQUIRE(!result.value);
        REQUIRE(result.key_id.empty());
    };

    expect("", Reason::malformed);
    expect("hello", Reason::malformed);
    expect(std::string(sf::max_file_size + 1, 'x'), Reason::malformed);

    const std::string text = signed_text(annual());
    const auto format = text.find("\"format\":1");
    REQUIRE(format != std::string::npos);
    std::string newer = text;
    newer.replace(format, 10, "\"format\":2");
    expect(newer, Reason::newer_version);

    // One character of the payload changed.
    std::string tampered = text;
    const auto payload = tampered.find("\"payload\":\"") + 11;
    tampered[payload + 20] = tampered[payload + 20] == 'A' ? 'B' : 'A';
    expect(tampered, Reason::bad_signature);

    // Signed, but not as a licence.
    expect(signed_text(annual(), revocations_purpose), Reason::malformed);

    // Signed, and the payload is not one this version reads.
    expect(signed_payload("{\"licence_id\":\"ADL-1\"}"), Reason::malformed);
    expect(signed_payload(payload_with("\"critical\":[]", "\"critical\":[\"future\"]")), Reason::newer_version);

    // A key this build does not know.
    Context no_keys = where;
    no_keys.keys = {};
    const Status unknown = evaluate(text, no_keys);
    REQUIRE(unknown.state == State::invalid);
    REQUIRE(unknown.reason == Reason::newer_version);
}

TEST_CASE("licence: a product build refuses a licence signed with the test key", "[licence][state]")
{
    // The product's list holds ADL's own keys and no test key, so the test key is one this release does not
    // know, and a program reads that as a licence made for a newer version.
    const auto keys = sf::product_trust_list();
    Context where = on(at(2027, 1, 1));
    where.keys = keys;
    const Status result = evaluate(signed_text(annual()), where);
    REQUIRE(result.state == State::invalid);
    REQUIRE(result.reason == Reason::newer_version);
    REQUIRE(!result.active());
    REQUIRE(!result.value);
    REQUIRE(!check(result, "support").granted);

    // A licence signed with the test key under the id of a key a release does trust is refused for its
    // signature.
    sf::TrustedKey real = test_keys::trusted()[0];
    real.key_id = "lic-2026-a";
    real.key[0] ^= 0x01U;
    const std::vector<sf::TrustedKey> release{real};
    where.keys = release;
    const auto payload = write_payload(annual());
    REQUIRE(payload);
    const std::string forged =
        sf::write(signing::sign(licence_purpose, "lic-2026-a", span_of(*payload), test_keys::private_key()));
    const Status refused = evaluate(forged, where);
    REQUIRE(refused.state == State::invalid);
    REQUIRE(refused.reason == Reason::bad_signature);
    REQUIRE(!refused.value);
    REQUIRE(!check(refused, "support").granted);
}

TEST_CASE("licence: a licence for another product is refused", "[licence][state]")
{
    Licence other = annual();
    other.product = "vss";
    const std::string text = signed_text(other);

    const Status refused = evaluate(text, on(at(2027, 1, 1)));
    REQUIRE(refused.state == State::invalid);
    REQUIRE(refused.reason == Reason::wrong_product);
    REQUIRE(!refused.active());
    // The signature held, so a window can say whose licence it is and which product it is for.
    REQUIRE(refused.value);
    REQUIRE(refused.value->product == "vss");
    REQUIRE(refused.key_id == test_keys::key_id);
    REQUIRE(!check(refused, "support").granted);

    // The same file is fine to the product it names.
    Context vss = on(at(2027, 1, 1));
    vss.product = "vss";
    REQUIRE(evaluate(text, vss).state == State::valid);

    // And a licence for this product is refused by the other.
    REQUIRE(evaluate(signed_text(annual()), vss).reason == Reason::wrong_product);
}

TEST_CASE("licence: revocation by licence id and by key", "[licence][revocation]")
{
    const std::string text = signed_text(annual());
    Context where = on(at(2027, 1, 1));
    REQUIRE(evaluate(text, where).state == State::valid);

    RevocationList list;
    list.sequence = 4;
    list.licence_ids = {"ADL-2026-000001", "ADL-2026-000123"};
    where.revocations = &list;
    const Status result = evaluate(text, where);
    REQUIRE(result.state == State::invalid);
    REQUIRE(result.reason == Reason::revoked);
    // The window can still say whose licence was withdrawn.
    REQUIRE(result.value);
    REQUIRE(!check(result, "support").granted);

    list.licence_ids = {"ADL-2026-000001"};
    REQUIRE(evaluate(text, where).state == State::valid);

    // The key leaked after this licence was issued: it keeps working.
    list.keys = {{std::string(test_keys::key_id), at(2026, 12, 1)}};
    REQUIRE(evaluate(text, where).state == State::valid);
    // The key leaked before it was issued.
    list.keys = {{std::string(test_keys::key_id), at(2026, 11, 1)}};
    REQUIRE(evaluate(text, where).reason == Reason::revoked);
    // The key is withdrawn outright.
    list.keys = {{std::string(test_keys::key_id), std::nullopt}};
    REQUIRE(evaluate(text, where).reason == Reason::revoked);
    // Another key's withdrawal does not touch it.
    list.keys = {{"lic-2020-a", std::nullopt}};
    REQUIRE(evaluate(text, where).state == State::valid);

    // Judged without a key (a test of the payload alone), only the licence number can withdraw it.
    list.keys = {{std::string(test_keys::key_id), std::nullopt}};
    REQUIRE(evaluate(annual(), nullptr, where).state == State::valid);
    list.licence_ids = {"ADL-2026-000123"};
    REQUIRE(evaluate(annual(), nullptr, where).reason == Reason::revoked);
}

TEST_CASE("licence: a key's window and withdrawal in the trust list", "[licence][revocation]")
{
    const std::string text = signed_text(annual());
    std::vector<sf::TrustedKey> keys(test_keys::trusted().begin(), test_keys::trusted().end());
    Context where = on(at(2027, 1, 1));
    where.keys = keys;

    keys[0].not_after = at(2026, 11, 1);
    Status result = evaluate(text, where);
    REQUIRE(result.state == State::invalid);
    REQUIRE(result.reason == Reason::revoked);
    REQUIRE(result.value);

    keys[0].not_after = TimePoint::max();
    keys[0].revoked_from = at(2026, 11, 13);
    REQUIRE(evaluate(text, where).reason == Reason::revoked);
    keys[0].revoked_from = at(2026, 11, 14);
    REQUIRE(evaluate(text, where).state == State::valid);
    keys[0].revoked_from = TimePoint::min();
    result = evaluate(text, where);
    REQUIRE(result.reason == Reason::revoked);
    REQUIRE(!result.value);
}

TEST_CASE("licence: revocation lists", "[licence][revocation]")
{
    RevocationList list;
    list.sequence = 7;
    list.issued_at = at(2027, 2, 1);
    list.licence_ids = {"ADL-2026-000123"};
    list.keys = {{"lic-2026-a", at(2027, 1, 15)}, {"lic-2025-a", std::nullopt}};

    const auto payload = write_revocations(list);
    REQUIRE(payload);
    const auto parsed = parse_revocations(span_of(*payload));
    REQUIRE(parsed);
    REQUIRE(*parsed == list);
    // Ids only: nothing in the list can hold a name.
    REQUIRE(
        !parse_revocations(span_of(R"({"sequence":1,"issued_at":"2027-02-01T00:00:00Z","licences":["Example Lab"]})")));
    REQUIRE(!parse_revocations(span_of("{}")));
    REQUIRE(!parse_revocations(span_of("[]")));
    REQUIRE(!parse_revocations(span_of(R"({"sequence":-1,"issued_at":"2027-02-01T00:00:00Z"})")));
    REQUIRE(!parse_revocations(span_of(R"({"sequence":1,"issued_at":"2027-02-01T00:00:00Z","keys":{"key_id":"a"}})")));
    REQUIRE(!parse_revocations(span_of(R"({"sequence":1,"issued_at":"2027-02-01T00:00:00Z","keys":[7]})")));
    REQUIRE(
        !parse_revocations(span_of(R"({"sequence":1,"issued_at":"2027-02-01T00:00:00Z","keys":[{"key_id":"A B"}]})")));
    REQUIRE(!parse_revocations(
        span_of(R"({"sequence":1,"issued_at":"2027-02-01T00:00:00Z","keys":[{"key_id":"a","from":"soon"}]})")));
    const auto future =
        parse_revocations(span_of(R"({"sequence":1,"issued_at":"2027-02-01T00:00:00Z","critical":["x"]})"));
    REQUIRE(!future);
    REQUIRE(future.error() == Error::newer_version);

    const auto sign_list = [&](std::string_view purpose)
    { return sf::write(signing::sign(purpose, test_keys::key_id, span_of(*payload), test_keys::private_key())); };
    const auto read = read_revocations(sign_list(revocations_purpose), test_keys::trusted());
    REQUIRE(read);
    REQUIRE(*read == list);

    // A licence signature is not a revocation signature.
    const auto wrong = read_revocations(sign_list(licence_purpose), test_keys::trusted());
    REQUIRE(!wrong);
    REQUIRE(wrong.error() == Error::wrong_purpose);
    REQUIRE(!read_revocations("junk", test_keys::trusted()));
    REQUIRE(!read_revocations(sign_list(revocations_purpose), {}));
    // Signed, as a list, with a payload that is not one.
    REQUIRE(!read_revocations(signed_payload("{}", revocations_purpose), test_keys::trusted()));

    std::vector<sf::TrustedKey> keys(test_keys::trusted().begin(), test_keys::trusted().end());
    keys[1].not_after = at(2027, 1, 1);
    const auto late = read_revocations(sign_list(revocations_purpose), keys);
    REQUIRE(!late);
    REQUIRE(late.error() == Error::key_not_valid);

    // The program never goes back to an older list.
    RevocationList older = list;
    older.sequence = 6;
    older.licence_ids.clear();
    REQUIRE(newer(list, older).sequence == 7U);
    REQUIRE(newer(older, list).sequence == 7U);
    REQUIRE(newer(list, list).sequence == 7U);

    // A list that is written is one that reads back; one that would not is refused.
    RevocationList bad = list;
    bad.licence_ids = {"not a number"};
    REQUIRE(!write_revocations(bad));
}

TEST_CASE("licence: the list built into this release withdraws nothing yet", "[licence][revocation]")
{
    const RevocationList built_in = builtin_revocations();
    REQUIRE(built_in.sequence == 0U);
    REQUIRE(built_in.licence_ids.empty());
    REQUIRE(built_in.keys.empty());

    // Any signed list is newer than it, so a file a customer is sent takes its place.
    RevocationList sent;
    sent.sequence = 1;
    sent.licence_ids = {"ADL-2026-000123"};
    REQUIRE(newer(built_in, sent).sequence == 1U);
    REQUIRE(newer(sent, built_in).sequence == 1U);
}

TEST_CASE("licence: computer names", "[licence][state]")
{
    REQUIRE(machine_matches("LAB-RIG-07", "LAB-RIG-07"));
    REQUIRE(machine_matches("lab-rig-07", "LAB-RIG-07"));
    REQUIRE(machine_matches("LAB-RIG-*", "lab-rig-07"));
    REQUIRE(machine_matches("LAB-RIG-*", "LAB-RIG-"));
    REQUIRE(!machine_matches("LAB-RIG-07", "LAB-RIG-070"));
    REQUIRE(!machine_matches("LAB-RIG-07", "LAB-RIG-0"));
    REQUIRE(!machine_matches("LAB-RIG-*", "LAB-BENCH-1"));
    REQUIRE(!machine_matches("LAB-RIG-07", ""));

    Licence value = annual();
    value.edition = Edition::enterprise;
    value.kind = LicenceKind::enterprise;
    value.seats = 0;
    value.machines = {"BENCH-1", "LAB-RIG-*"};
    const std::string text = signed_text(value);

    Context where = on(at(2027, 1, 1));
    REQUIRE(evaluate(text, where).state == State::valid);
    where.computer_name = "bench-1";
    REQUIRE(evaluate(text, where).state == State::valid);
    where.computer_name = "OFFICE-PC";
    const Status result = evaluate(text, where);
    REQUIRE(result.state == State::invalid);
    REQUIRE(result.reason == Reason::wrong_computer);
    // The window lists the names the licence is for.
    REQUIRE(result.value);
    REQUIRE(result.value->machines.size() == 2);
    where.computer_name.clear();
    REQUIRE(evaluate(text, where).reason == Reason::wrong_computer);
}

TEST_CASE("licence: the clock check", "[licence][clock]")
{
    const std::string text = signed_text(annual());

    // The program has seen 1 March. A clock inside the tolerance is fine.
    Context where = on(at(2027, 2, 27));
    where.latest_seen = at(2027, 3, 1);
    REQUIRE(evaluate(text, where).state == State::valid);
    where.now = at(2027, 2, 27) - seconds{1};
    const Status result = evaluate(text, where);
    REQUIRE(result.state == State::clock_suspect);
    REQUIRE(result.reason == Reason::clock_suspect);
    REQUIRE(!result.active());
    REQUIRE(!check(result, "support").granted);
    REQUIRE(check(result, "support").reason == Reason::clock_suspect);

    // Putting the clock right clears it.
    where.now = at(2027, 3, 2);
    REQUIRE(evaluate(text, where).state == State::valid);

    // A clock set back past the end of the term to keep an ended licence alive: the stored time says
    // otherwise.
    where.now = at(2027, 6, 1);
    where.latest_seen = at(2028, 2, 1);
    REQUIRE(evaluate(text, where).state == State::clock_suspect);

    // With nothing stored, the build date and the signing time still bound the clock.
    Context fresh = on(at(2026, 6, 1));
    REQUIRE(evaluate(text, fresh).state == State::clock_suspect);
    fresh.now = at(2027, 1, 1);
    fresh.build_time = at(2027, 3, 1);
    REQUIRE(evaluate(text, fresh).state == State::clock_suspect);
    fresh.build_time = at(2027, 1, 2);
    REQUIRE(evaluate(text, fresh).state == State::valid);
}

TEST_CASE("licence: the stored time moves forward, and a reset works once", "[licence][clock]")
{
    ClockState stored;
    stored = observe(stored, at(2027, 1, 1));
    REQUIRE(stored.latest_seen == at(2027, 1, 1));
    stored = observe(stored, at(2026, 1, 1));
    REQUIRE(stored.latest_seen == at(2027, 1, 1));

    // The clock once read 2031. An ordinary licence does not move it back.
    stored = observe(stored, at(2031, 1, 1));
    REQUIRE(apply_reset(stored, annual()) == stored);

    Licence reset = annual();
    reset.issued_at = at(2027, 1, 5);
    reset.clock_reset = "reset-000123-a";
    const ClockState after = apply_reset(stored, reset);
    REQUIRE(after.latest_seen == at(2027, 1, 5));
    REQUIRE(after.applied_resets.size() == 1);

    // The clock check passes again with the stored time put right.
    Context where = on(at(2027, 1, 6));
    where.latest_seen = stored.latest_seen;
    REQUIRE(evaluate(signed_text(reset), where).state == State::clock_suspect);
    where.latest_seen = after.latest_seen;
    REQUIRE(evaluate(signed_text(reset), where).state == State::valid);

    // Importing the same file again later does nothing.
    const ClockState later = observe(after, at(2027, 8, 1));
    REQUIRE(apply_reset(later, reset) == later);

    // The list of used tokens stays bounded.
    ClockState many;
    for (int i = 0; i < 40; ++i)
    {
        Licence next = reset;
        next.clock_reset = "reset-" + std::to_string(i);
        many = apply_reset(many, next);
    }
    REQUIRE(many.applied_resets.size() == 32);
    REQUIRE(many.applied_resets.back() == "reset-39");
}

TEST_CASE("licence: the licensee's email address is carried and checked", "[licence][payload]")
{
    // A licence without one is written without the field.
    const auto without = write_payload(annual());
    REQUIRE(without);
    REQUIRE(without->find("\"email\"") == std::string::npos);
    REQUIRE(parse_payload(span_of(*without))->licensee_email.empty());

    Licence named = annual();
    named.licensee_email = "tests@example-lab.invalid";
    const auto written = write_payload(named);
    REQUIRE(written);
    REQUIRE(written->find("\"email\":\"tests@example-lab.invalid\"") != std::string::npos);
    const auto read = parse_payload(span_of(*written));
    REQUIRE(read);
    REQUIRE(*read == named);

    // It is signed with the rest: a licence judged from a file carries it.
    const Status judged = evaluate(signed_text(named), on(at(2027, 1, 1)));
    REQUIRE(judged.state == State::valid);
    REQUIRE(judged.value.has_value());
    REQUIRE(judged.value->licensee_email == "tests@example-lab.invalid");

    for (const std::string_view good :
         {"a@b.co", "first.last+tag@sub.example-lab.invalid", "o'brien_99@example.invalid"})
    {
        REQUIRE(is_email_address(good));
    }
    for (const std::string_view bad :
         {"", "no-at.example.invalid", "@example.invalid", "a@", "a@b", "a@@b.co", "a@b@c.co", "a b@example.invalid",
          "a@exa mple.invalid", "a@.example.invalid", "a@example.invalid.", "a@-example.invalid", "a@example..invalid",
          "a@example.invalid-", "<a@example.invalid>", "a@example.invalid\n", "\"a\"@example.invalid"})
    {
        REQUIRE(!is_email_address(bad));
    }
    REQUIRE(!is_email_address(std::string(250, 'a') + "@b.co"));

    // A licence with an address that has not that form is refused, to write and to read.
    Licence wrong = annual();
    wrong.licensee_email = "not an address";
    REQUIRE(!write_payload(wrong));
    expect_payload_error(
        payload_with("\"contact\":\"Test team\"", "\"contact\":\"Test team\",\"email\":\"not an address\""),
        Error::malformed);
    expect_payload_error(payload_with("\"contact\":\"Test team\"", "\"contact\":\"Test team\",\"email\":7"),
                         Error::malformed);
}

TEST_CASE("licence: the version window keeps what was released in time", "[licence][entitlement]")
{
    // A licence for 13 November 2026 to 13 November 2027 whose window ends with its term.
    Licence windowed = annual();
    windowed.updates_until = windowed.expires_at;
    const std::string text = signed_text(windowed);
    const TimePoint in_time = at(2027, 6, 1);
    const TimePoint last_day = at(2027, 11, 13);
    const TimePoint too_late = at(2027, 11, 13, 1);

    // While the licence runs, and in its grace period, the release date changes nothing: everything it
    // names is granted in the ordinary way.
    for (const TimePoint now : {at(2027, 1, 1), at(2027, 12, 1)})
    {
        const Status running = evaluate(text, on(now));
        for (const TimePoint released : {in_time, too_late})
        {
            const Entitlement answer = check(running, "support", released);
            REQUIRE(answer.granted);
            REQUIRE(!answer.by_version_window);
        }
    }

    // After the end: released within the window still works, for good.
    const Status ended = evaluate(text, on(at(2030, 1, 1)));
    REQUIRE(ended.state == State::expired);
    REQUIRE(!check(ended, "support").granted);
    Entitlement answer = check(ended, "support", in_time);
    REQUIRE(answer.granted);
    REQUIRE(answer.reason == Reason::ok);
    REQUIRE(answer.by_version_window);
    REQUIRE(answer.licence_id == "ADL-2026-000123");
    REQUIRE(answer.days_left == 0);
    REQUIRE(check(ended, "support", last_day).granted);

    // Released after the window: a current licence is needed.
    answer = check(ended, "support", too_late);
    REQUIRE(!answer.granted);
    REQUIRE(answer.reason == Reason::expired);
    REQUIRE(!answer.by_version_window);

    // The window covers only what the licence names.
    answer = check(ended, "data_packs", in_time);
    REQUIRE(!answer.granted);
    REQUIRE(answer.reason == Reason::expired);

    // A licence without the field ends as the states table says.
    const Status plain = evaluate(signed_text(annual()), on(at(2030, 1, 1)));
    REQUIRE(!check(plain, "support", in_time).granted);

    // The window is for a licence that has ended and nothing else. One that is withdrawn, for other
    // computers, not started, or judged while the clock looks wrong grants nothing through it.
    RevocationList withdrawn;
    withdrawn.sequence = 1;
    withdrawn.licence_ids = {"ADL-2026-000123"};
    Context revoked = on(at(2030, 1, 1));
    revoked.revocations = &withdrawn;
    REQUIRE(!check(evaluate(text, revoked), "support", in_time).granted);

    Licence elsewhere = windowed;
    elsewhere.machines = {"OFFICE-*"};
    REQUIRE(!check(evaluate(signed_text(elsewhere), on(at(2030, 1, 1))), "support", in_time).granted);

    REQUIRE(!check(evaluate(text, on(at(2026, 11, 12, 12))), "support", in_time).granted);

    Context set_back = on(at(2030, 1, 1));
    set_back.latest_seen = at(2031, 1, 1);
    const Status suspect = evaluate(text, set_back);
    REQUIRE(suspect.state == State::clock_suspect);
    REQUIRE(!check(suspect, "support", in_time).granted);

    REQUIRE(!check(no_licence(), "support", in_time).granted);
}

TEST_CASE("licence: features", "[licence][entitlement]")
{
    REQUIRE(feature_matches("support", "support"));
    REQUIRE(!feature_matches("support", "supports"));
    REQUIRE(!feature_matches("support", "supp"));
    REQUIRE(feature_matches("pro.*", "pro.automation"));
    REQUIRE(feature_matches("pro.*", "pro.a.b"));
    REQUIRE(!feature_matches("pro.*", "pro"));
    REQUIRE(!feature_matches("pro.*", "pro."));
    REQUIRE(!feature_matches("pro.*", "professional.automation"));

    // The names this product asks about are in the form a licence may carry.
    REQUIRE(feature_support == "support");
    REQUIRE(feature_data_packs == "data_packs");
    Licence carrying = annual();
    carrying.features = {std::string(feature_support), std::string(feature_data_packs)};
    const auto carried = write_payload(carrying);
    REQUIRE(carried);
    REQUIRE(parse_payload(span_of(*carried)));

    const std::string text = signed_text(full());
    const Status valid = evaluate(text, on(at(2027, 1, 1)));
    REQUIRE(valid.state == State::valid);
    Entitlement answer = check(valid, "support");
    REQUIRE(answer.granted);
    REQUIRE(answer.reason == Reason::ok);
    REQUIRE(answer.licence_id == "ADL-2026-000124");
    REQUIRE(check(valid, "data_packs").granted);
    REQUIRE(check(valid, "pro.rig").granted);
    REQUIRE(check(valid, "enterprise.conformance").granted);

    answer = check(valid, "policy.official-only");
    REQUIRE(!answer.granted);
    REQUIRE(answer.reason == Reason::not_in_licence);

    // In the grace period the features still work.
    const std::string annual_text = signed_text(annual());
    const Status grace = evaluate(annual_text, on(at(2027, 12, 1)));
    REQUIRE(check(grace, "support").granted);
    REQUIRE(check(grace, "support").days_left == 10);
    // The edition alone grants nothing: a Pro licence that does not list data packs does not carry them.
    REQUIRE(!check(grace, "data_packs").granted);

    const Status expired = evaluate(annual_text, on(at(2028, 1, 1)));
    answer = check(expired, "support");
    REQUIRE(!answer.granted);
    REQUIRE(answer.reason == Reason::expired);
    REQUIRE(answer.licence_id == "ADL-2026-000123");

    const Status early = evaluate(annual_text, on(at(2026, 11, 12, 12)));
    REQUIRE(check(early, "support").reason == Reason::not_yet_valid);

    answer = check(no_licence(), "support");
    REQUIRE(!answer.granted);
    REQUIRE(answer.reason == Reason::no_licence);
    REQUIRE(answer.licence_id.empty());

    answer = check(evaluate("junk", on(at(2027, 1, 1))), "support");
    REQUIRE(!answer.granted);
    REQUIRE(answer.reason == Reason::malformed);
}

TEST_CASE("licence: the best of several files", "[licence][state]")
{
    REQUIRE(best({}).state == State::none);
    REQUIRE(best({}).reason == Reason::no_licence);

    const Context where = on(at(2027, 12, 1));
    // In its grace period on 1 December 2027.
    const Status old_pro = evaluate(signed_text(annual()), where);
    REQUIRE(old_pro.state == State::grace);

    Licence renewal = annual();
    renewal.licence_id = "ADL-2027-000050";
    renewal.issued_at = at(2027, 11, 1);
    renewal.starts_at = at(2027, 11, 13);
    renewal.expires_at = at(2028, 11, 13);
    renewal.replaces = "ADL-2026-000123";
    const Status new_pro = evaluate(signed_text(renewal), where);
    REQUIRE(new_pro.state == State::valid);

    Licence site = renewal;
    site.licence_id = "ADL-2027-000051";
    site.edition = Edition::enterprise;
    site.kind = LicenceKind::enterprise;
    site.expires_at = at(2028, 6, 1);
    const Status enterprise = evaluate(signed_text(site), where);

    const Status broken = evaluate("junk", where);
    const Status ended = evaluate(signed_text(annual()), on(at(2029, 1, 1)));
    REQUIRE(ended.state == State::expired);

    const auto id = [](const Status& s) { return s.value->licence_id; };
    std::vector<Status> files{broken, old_pro, new_pro};
    REQUIRE(id(best(files)) == "ADL-2027-000050");
    // Enterprise beats Pro, even with the earlier end.
    files.push_back(enterprise);
    REQUIRE(id(best(files)) == "ADL-2027-000051");
    // Grace beats ended and broken.
    files = {ended, broken, old_pro};
    REQUIRE(best(files).state == State::grace);
    // An ended licence is shown in preference to a broken file.
    files = {broken, ended};
    REQUIRE(best(files).state == State::expired);
    files = {broken};
    REQUIRE(best(files).state == State::invalid);

    // Two valid Pro licences: the later end wins.
    Licence longer = renewal;
    longer.licence_id = "ADL-2027-000052";
    longer.expires_at = at(2029, 1, 1);
    files = {new_pro, evaluate(signed_text(longer), where)};
    REQUIRE(id(best(files)) == "ADL-2027-000052");

    // Every state ranks: a licence that has not started beats a clock that looks wrong, which beats an
    // ended one, which beats a broken file.
    Context early = on(at(2026, 11, 12, 12));
    const Status not_started = evaluate(signed_text(annual()), early);
    REQUIRE(not_started.state == State::not_yet_valid);
    Context set_back = on(at(2027, 3, 1));
    set_back.latest_seen = at(2028, 3, 1);
    const Status suspect = evaluate(signed_text(annual()), set_back);
    REQUIRE(suspect.state == State::clock_suspect);
    files = {broken, ended, suspect, not_started};
    REQUIRE(best(files).state == State::not_yet_valid);
    files = {broken, ended, suspect};
    REQUIRE(best(files).state == State::clock_suspect);
}
