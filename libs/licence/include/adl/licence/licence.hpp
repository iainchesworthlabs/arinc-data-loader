// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// Licence files: the payload, the state a licence is in and what it grants. Nothing here reads a clock, a
// file or the computer's name: the caller passes them in, so every state can be tested.
//
// A licence file is a record of what was bought. It does not change the terms the code is under, and
// no part of the programs depends on one: the check shows the edition and gates nothing yet.

#include "adl/licence/error.hpp"
#include "adl/licence/product.hpp"
#include "adl/licence/signed_file.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::licence
{

using TimePoint = signed_file::TimePoint;

inline constexpr std::string_view licence_purpose = "licence";
inline constexpr std::string_view revocations_purpose = "revocations";

// A clock this far behind the latest time seen is still accepted. It covers time zones, daylight saving
// and a dual-boot machine that keeps local time.
inline constexpr std::chrono::hours clock_tolerance{48};

// A licence with this many days left, or fewer, is `expiring`.
inline constexpr int expiring_days = 30;
inline constexpr int expiring_days_trial = 3;

// The longest feature name a licence may list, not counting a trailing ".*".
inline constexpr std::size_t max_feature_length = 136;

inline constexpr int max_grace_days = 366;
inline constexpr int max_seats = 1'000'000;

enum class LicenceKind
{
    trial,
    annual,
    enterprise,
};

enum class Edition
{
    pro,
    enterprise,
};

// The payload of a licence file.
struct Licence
{
    // Which product the licence is for ("adl"). A licence for another product is refused.
    std::string product;
    std::string licence_id;
    LicenceKind kind = LicenceKind::annual;
    Edition edition = Edition::pro;
    std::string licensee_name;
    // Optional. A team or role, not a personal address.
    std::string licensee_contact;
    // Optional. The email address the licensee gave when they asked for the licence. It is personal data:
    // a screen shows it to the person whose computer holds the licence, and nothing else writes it.
    std::string licensee_email;
    TimePoint issued_at;
    TimePoint starts_at;
    TimePoint expires_at;
    // Days after `expires_at` during which the features still work.
    int grace_days = 0;
    // 0 is a site licence with no seat count. The terms say what a seat is; nothing counts installations.
    int seats = 1;
    // Exact names, or a prefix that ends in ".*".
    std::vector<std::string> features;
    // Computer names, where a trailing '*' matches a prefix. Empty means any computer.
    std::vector<std::string> machines;
    // The version window: what was released on or before this date keeps working after the licence ends.
    std::optional<TimePoint> updates_until;
    // The licence this one renews or corrects. Empty when there is none.
    std::string replaces;
    // Optional. A licence that carries a token the program has not applied before sets the stored latest
    // time back to `issued_at`, once. The owner sends one to a customer whose clock once ran far ahead.
    std::string clock_reset;

    friend bool operator==(const Licence&, const Licence&) = default;
};

// Reads a payload. Unknown fields are ignored; a field named in `critical` that this version does not
// know gives Error::newer_version. Any other fault is Error::malformed, with no detail, so that a bad
// file says nothing about how close it came.
[[nodiscard]] Result<Licence> parse_payload(std::span<const std::uint8_t> payload);

// The payload text for a licence, as `parse_payload` reads it. Refuses a licence `parse_payload` would
// refuse.
[[nodiscard]] Result<std::string> write_payload(const Licence& value);

// Whether `text` has the form of an email address a licence may carry: at most 254 ASCII characters, one
// '@', something before it, and after it a name with a dot that neither starts nor ends with a dot or a
// hyphen. It does not show that the address exists.
[[nodiscard]] bool is_email_address(std::string_view text) noexcept;

// "2026-11-13T02:10:00Z", the only form a payload uses.
[[nodiscard]] Result<TimePoint> parse_time(std::string_view text);
[[nodiscard]] std::string format_time(TimePoint value);

// A key that a revocation list withdraws.
struct RevokedKey
{
    std::string key_id;
    // Files signed at or after this time are refused. Absent withdraws the key outright.
    std::optional<TimePoint> from;

    friend bool operator==(const RevokedKey&, const RevokedKey&) = default;
};

// The payload of a revocation list. It holds ids only, never names.
struct RevocationList
{
    // The program keeps the list with the highest number it has seen.
    std::uint64_t sequence = 0;
    TimePoint issued_at;
    std::vector<std::string> licence_ids;
    std::vector<RevokedKey> keys;

    friend bool operator==(const RevocationList&, const RevocationList&) = default;
};

[[nodiscard]] Result<RevocationList> parse_revocations(std::span<const std::uint8_t> payload);
[[nodiscard]] Result<std::string> write_revocations(const RevocationList& value);

// Verifies and reads a signed revocation list file.
[[nodiscard]] Result<RevocationList> read_revocations(std::string_view file_text,
                                                      std::span<const signed_file::TrustedKey> keys);

// The revocation list built into this release (revocations_release.cpp). Empty, with sequence 0, until
// the owner withdraws a licence or a key. A signed list with a higher sequence number takes its place.
[[nodiscard]] RevocationList builtin_revocations();

// The newer of the two lists; `current` when `candidate` is not newer.
[[nodiscard]] RevocationList newer(const RevocationList& current, const RevocationList& candidate);

enum class State
{
    none, // no licence file
    valid,
    expiring,      // valid, and close to its end
    grace,         // past `expires_at`, within the grace period: the features still work
    expired,       // past the grace period
    not_yet_valid, // before `starts_at`
    clock_suspect, // the clock reads well before a time the program knows has passed
    invalid,       // see the reason
};

// Why a licence is in its state, or why a feature is not granted. A screen maps each to one sentence.
enum class Reason
{
    ok,
    no_licence,
    expired,
    not_yet_valid,
    clock_suspect,
    malformed,     // damaged, incomplete or not a licence at all
    newer_version, // needs a newer version: an unknown format, key or critical field
    bad_signature,
    revoked, // the licence or its key was withdrawn
    wrong_computer,
    wrong_product,  // the licence is for another product
    not_in_licence, // the licence is fine and does not include the feature asked for
};

// The reason a file that could not be read or judged amounts to: what a screen says about it.
[[nodiscard]] Reason reason_for(Error error) noexcept;

// What the program knows when it judges a licence. All of it is passed in.
struct Context
{
    TimePoint now;
    // The latest time the program has seen, from its settings. TimePoint::min() when there is none.
    TimePoint latest_seen = TimePoint::min();
    // When this program was built. Another time that has certainly passed.
    TimePoint build_time = TimePoint::min();
    std::string computer_name;
    std::string_view product = product_id;
    std::span<const signed_file::TrustedKey> keys;
    // May be null.
    const RevocationList* revocations = nullptr;
};

struct Status
{
    State state = State::none;
    Reason reason = Reason::no_licence;
    // Present when the signature verified and the payload was read, even for an ended or withdrawn
    // licence, so that a screen can show whose it is.
    std::optional<Licence> value;
    // The key that signed it. Empty unless the signature verified.
    std::string key_id;
    // Whole days until `expires_at` (valid, expiring) or until the end of the grace period (grace). 0 is
    // "today". 0 in every other state.
    int days_left = 0;
    // `expires_at` plus the grace period. Meaningful when `value` is set.
    TimePoint grace_ends;

    // Whether the licence's features work now.
    [[nodiscard]] bool active() const noexcept;
};

// The status when there is no licence file.
[[nodiscard]] Status no_licence();

// Judges the text of one licence file.
[[nodiscard]] Status evaluate(std::string_view file_text, const Context& where);

// Judges a licence whose signature was already checked. `signed_by` may be null in a test; then the key
// checks are skipped.
[[nodiscard]] Status evaluate(const Licence& value, const signed_file::TrustedKey* signed_by, const Context& where);

// The status the program should use out of several files: active beats inactive, valid beats grace,
// Enterprise beats Pro, then the later expiry. no_licence() for an empty list.
[[nodiscard]] Status best(std::span<const Status> candidates);

struct Entitlement
{
    bool granted = false;
    Reason reason = Reason::no_licence;
    // Empty when there is no readable licence.
    std::string licence_id;
    int days_left = 0;
    // Granted by the version window: the licence has ended, and what was asked about was released on or
    // before its `updates_until`.
    bool by_version_window = false;
};

// Whether the licence behind `current` turns `feature` on, and why not.
[[nodiscard]] Entitlement check(const Status& current, std::string_view feature);

// The same question for something with a release date. A licence that sets `updates_until` keeps what was
// released on or before that date working after the licence has ended. This applies to an ended licence
// only: one that is withdrawn, for another computer or product, not started, or judged while the clock
// looks wrong grants nothing here either. A licence without `updates_until` gives the answer of `check`
// above.
[[nodiscard]] Entitlement check(const Status& current, std::string_view feature, TimePoint released);

// Whether a `features` entry covers a feature name: the same name, or a prefix ending in ".*".
[[nodiscard]] bool feature_matches(std::string_view entry, std::string_view feature);

// Whether a `machines` entry names a computer. Letters compare without regard to ASCII case; a trailing
// '*' matches any ending.
[[nodiscard]] bool machine_matches(std::string_view entry, std::string_view computer_name);

// The most `clock_reset` tokens the stored clock state remembers. The oldest is forgotten first.
inline constexpr std::size_t max_applied_resets = 32;

// What the program stores between runs for the clock check.
struct ClockState
{
    TimePoint latest_seen = TimePoint::min();
    // The `clock_reset` tokens already used, so that a file works once.
    std::vector<std::string> applied_resets;

    friend bool operator==(const ClockState&, const ClockState&) = default;
};

// The stored state after the program has seen the time `now`: the latest time only ever moves forward here.
[[nodiscard]] ClockState observe(ClockState stored, TimePoint now);

// The stored state after a verified licence is imported. A `clock_reset` token that has not been applied
// sets the latest time to the licence's `issued_at`. Anything else leaves the state as it was.
[[nodiscard]] ClockState apply_reset(ClockState stored, const Licence& imported);

} // namespace adl::licence
