// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence/licence.hpp"

#include "json.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <tuple>
#include <utility>
#include <variant>

namespace adl::licence
{

namespace
{

using std::chrono::days;

constexpr std::size_t max_id_length = 64;
constexpr std::size_t max_product_length = 16;
constexpr std::size_t max_kind_length = 16;
constexpr std::size_t max_name_length = 200;
constexpr std::size_t max_email_length = 254;
constexpr std::size_t max_features = 64;
constexpr std::size_t max_machine_length = 64;
constexpr std::size_t max_machines = 1024;
constexpr std::size_t max_revoked = 100'000;
constexpr std::size_t max_applied_resets = 32;
constexpr std::size_t max_critical = 64;
constexpr std::size_t time_text_length = 20;
constexpr int first_year = 2000;

// The payload fields this version understands. A name in `critical` that is not here makes the licence
// one for a newer version.
constexpr std::array<std::string_view, 16> known_licence_fields{
    "product",    "licence_id", "kind",     "edition",  "licensee",      "issued_at", "starts_at",   "expires_at",
    "grace_days", "seats",      "features", "machines", "updates_until", "replaces",  "clock_reset", "critical"};

constexpr std::array<std::string_view, 5> known_revocation_fields{"sequence", "issued_at", "licences", "keys",
                                                                  "critical"};

[[nodiscard]] std::unexpected<Error> malformed()
{
    return std::unexpected(Error::malformed);
}

[[nodiscard]] bool is_upper_token(std::string_view text, std::size_t max_length) noexcept
{
    if (text.empty() || text.size() > max_length)
    {
        return false;
    }
    return std::ranges::all_of(text,
                               [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'; });
}

[[nodiscard]] bool is_feature_entry(std::string_view text) noexcept
{
    if (text.ends_with(".*"))
    {
        text.remove_suffix(2);
    }
    return detail::is_token(text, max_feature_length, ".-_") && text.front() != '.' && text.back() != '.';
}

[[nodiscard]] bool is_machine_entry(std::string_view text) noexcept
{
    if (text.ends_with('*'))
    {
        text.remove_suffix(1);
    }
    if (text.empty() || text.size() > max_machine_length)
    {
        return false;
    }
    return std::ranges::all_of(text,
                               [](char c)
                               {
                                   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                          c == '-' || c == '_' || c == '.';
                               });
}

// A name a person reads: no control characters.
[[nodiscard]] bool is_display_text(std::string_view text) noexcept
{
    return std::ranges::none_of(text,
                                [](char c)
                                {
                                    const auto byte = static_cast<unsigned char>(c);
                                    return byte < 0x20 || byte == 0x7f;
                                });
}

[[nodiscard]] char ascii_lower(char c) noexcept
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] bool digits(std::string_view text) noexcept
{
    return std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

[[nodiscard]] int number(std::string_view text) noexcept
{
    int value = 0;
    for (const char c : text)
    {
        value = value * 10 + (c - '0');
    }
    return value;
}

[[nodiscard]] Result<TimePoint> required_time(const detail::JsonObject& object, std::string_view key)
{
    const auto text = detail::required_string(object, key, time_text_length);
    if (!text)
    {
        return malformed();
    }
    return parse_time(*text);
}

// `critical` may only name fields this version knows.
[[nodiscard]] Result<void> check_critical(const detail::JsonObject& object, std::span<const std::string_view> known)
{
    const auto critical = detail::string_list(object, "critical", max_critical, max_critical);
    if (!critical)
    {
        return malformed();
    }
    for (const std::string& name : *critical)
    {
        if (std::ranges::find(known, name) == known.end())
        {
            return std::unexpected(Error::newer_version);
        }
    }
    return {};
}

[[nodiscard]] Result<detail::JsonObject> payload_object(std::span<const std::uint8_t> payload)
{
    const std::string_view text(reinterpret_cast<const char*>(payload.data()), payload.size());
    auto document = detail::parse_strict(text);
    if (!document || document->as_object() == nullptr)
    {
        return malformed();
    }
    return std::get<detail::JsonObject>(std::move(document->data));
}

[[nodiscard]] std::span<const std::uint8_t> as_bytes(std::string_view text) noexcept
{
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

[[nodiscard]] Reason reason_for(Error error) noexcept
{
    switch (error)
    {
    case Error::newer_version:
    case Error::unknown_key:
        return Reason::newer_version;
    case Error::key_not_valid:
        return Reason::revoked;
    case Error::bad_signature:
        return Reason::bad_signature;
    case Error::too_large:
    case Error::malformed:
    case Error::wrong_purpose:
        break;
    }
    return Reason::malformed;
}

[[nodiscard]] Status invalid(Reason why)
{
    Status status;
    status.state = State::invalid;
    status.reason = why;
    return status;
}

[[nodiscard]] bool revoked(const Licence& value, const signed_file::TrustedKey* signed_by, const RevocationList& list)
{
    if (std::ranges::find(list.licence_ids, value.licence_id) != list.licence_ids.end())
    {
        return true;
    }
    if (signed_by == nullptr)
    {
        return false;
    }
    return std::ranges::any_of(
        list.keys, [&](const RevokedKey& key)
        { return key.key_id == signed_by->key_id && (!key.from || value.issued_at >= *key.from); });
}

[[nodiscard]] int whole_days(TimePoint from, TimePoint until)
{
    return static_cast<int>(std::chrono::floor<days>(until - from).count());
}

[[nodiscard]] int state_rank(State value) noexcept
{
    switch (value)
    {
    case State::valid:
    case State::expiring:
        return 6;
    case State::grace:
        return 5;
    case State::not_yet_valid:
        return 4;
    case State::clock_suspect:
        return 3;
    case State::expired:
        return 2;
    case State::invalid:
        return 1;
    case State::none:
        return 0;
    }
    return 0;
}

[[nodiscard]] std::string_view kind_name(LicenceKind kind) noexcept
{
    switch (kind)
    {
    case LicenceKind::trial:
        return "trial";
    case LicenceKind::annual:
        return "annual";
    case LicenceKind::enterprise:
        return "enterprise";
    }
    return "annual";
}

void append_list(std::string& out, const std::vector<std::string>& items)
{
    out.push_back('[');
    bool first = true;
    for (const std::string& item : items)
    {
        if (!first)
        {
            out.push_back(',');
        }
        first = false;
        out += detail::quote(item);
    }
    out.push_back(']');
}

void append_member(std::string& out, std::string_view key, std::string_view quoted_value)
{
    out += detail::quote(key);
    out.push_back(':');
    out += quoted_value;
}

} // namespace

Result<TimePoint> parse_time(std::string_view text)
{
    // 2026-11-13T02:10:00Z
    if (text.size() != time_text_length || text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' ||
        text[16] != ':' || text[19] != 'Z' || !digits(text.substr(0, 4)) || !digits(text.substr(5, 2)) ||
        !digits(text.substr(8, 2)) || !digits(text.substr(11, 2)) || !digits(text.substr(14, 2)) ||
        !digits(text.substr(17, 2)))
    {
        return malformed();
    }
    const int year = number(text.substr(0, 4));
    const int hour = number(text.substr(11, 2));
    const int minute = number(text.substr(14, 2));
    const int second = number(text.substr(17, 2));
    const std::chrono::year_month_day date{std::chrono::year{year},
                                           std::chrono::month{static_cast<unsigned>(number(text.substr(5, 2)))},
                                           std::chrono::day{static_cast<unsigned>(number(text.substr(8, 2)))}};
    if (year < first_year || !date.ok() || hour > 23 || minute > 59 || second > 59)
    {
        return malformed();
    }
    return TimePoint{std::chrono::sys_days{date}} + std::chrono::hours{hour} + std::chrono::minutes{minute} +
           std::chrono::seconds{second};
}

std::string format_time(TimePoint value)
{
    const auto day = std::chrono::floor<days>(value);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{value - day};
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), "%04d-%02u-%02uT%02d:%02d:%02dZ", static_cast<int>(date.year()),
                  static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                  static_cast<int>(time.hours().count()), static_cast<int>(time.minutes().count()),
                  static_cast<int>(time.seconds().count()));
    return text.data();
}

Result<Licence> parse_payload(std::span<const std::uint8_t> payload)
{
    // A field this reader does not know, and that the file marks as critical, is a refusal, so that an
    // old reader never half understands a new file.
    const auto object = payload_object(payload);
    if (!object)
    {
        return malformed();
    }
    if (const auto critical = check_critical(*object, known_licence_fields); !critical)
    {
        return std::unexpected(critical.error());
    }

    Licence value;

    // Whose it is and what it is: the product, the number, the kind and the edition.
    const auto product = detail::required_string(*object, "product", max_product_length);
    const auto id = detail::required_string(*object, "licence_id", max_id_length);
    const auto kind = detail::required_string(*object, "kind", max_kind_length);
    const auto edition_text = detail::required_string(*object, "edition", max_kind_length);
    if (!product || !id || !kind || !edition_text || !detail::is_token(*product, max_product_length, "-") ||
        !is_upper_token(*id, max_id_length))
    {
        return malformed();
    }
    value.product = *product;
    value.licence_id = *id;

    if (*kind == "trial")
    {
        value.kind = LicenceKind::trial;
    }
    else if (*kind == "annual")
    {
        value.kind = LicenceKind::annual;
    }
    else if (*kind == "enterprise")
    {
        value.kind = LicenceKind::enterprise;
    }
    else
    {
        return malformed();
    }

    if (*edition_text == "pro")
    {
        value.edition = Edition::pro;
    }
    else if (*edition_text == "enterprise")
    {
        value.edition = Edition::enterprise;
    }
    else
    {
        return malformed();
    }

    // The licensee is a nested object with a name, an optional contact and an optional email address. The
    // first two are text for a person to read, so control characters are refused; the address has its
    // own form.
    const detail::JsonValue* licensee_member = detail::find(*object, "licensee");
    const detail::JsonObject* licensee = licensee_member == nullptr ? nullptr : licensee_member->as_object();
    if (licensee == nullptr)
    {
        return malformed();
    }
    const auto name = detail::required_string(*licensee, "name", max_name_length);
    const auto contact = detail::optional_string(*licensee, "contact", max_name_length);
    const auto email = detail::optional_string(*licensee, "email", max_email_length);
    if (!name || !contact || !email || !is_display_text(*name) || !is_display_text(*contact) ||
        (!email->empty() && !is_email_address(*email)))
    {
        return malformed();
    }
    value.licensee_name = *name;
    value.licensee_contact = *contact;
    value.licensee_email = *email;

    // The dates. A licence that ends before it starts is not valid.
    const auto issued = required_time(*object, "issued_at");
    const auto starts = required_time(*object, "starts_at");
    const auto expires = required_time(*object, "expires_at");
    if (!issued || !starts || !expires || *expires < *starts)
    {
        return malformed();
    }
    value.issued_at = *issued;
    value.starts_at = *starts;
    value.expires_at = *expires;

    // How many days of grace after the end, and how many seats.
    const auto grace = detail::required_integer(*object, "grace_days", 0, max_grace_days);
    const auto seats = detail::required_integer(*object, "seats", 0, max_seats);
    if (!grace || !seats)
    {
        return malformed();
    }
    value.grace_days = static_cast<int>(*grace);
    value.seats = static_cast<int>(*seats);

    // The features and the machines are lists of short text, each entry in the form that
    // `is_feature_entry` or `is_machine_entry` allows.
    auto features = detail::string_list(*object, "features", max_features, max_feature_length + 2);
    auto machines = detail::string_list(*object, "machines", max_machines, max_machine_length + 1);
    if (!features || !machines || !std::ranges::all_of(*features, is_feature_entry) ||
        !std::ranges::all_of(*machines, is_machine_entry))
    {
        return malformed();
    }
    value.features = std::move(*features);
    value.machines = std::move(*machines);

    // The optional fields follow. An empty text means the field was left out.
    const auto updates = detail::optional_string(*object, "updates_until", time_text_length);
    if (!updates)
    {
        return malformed();
    }
    if (!updates->empty())
    {
        const auto until = parse_time(*updates);
        if (!until)
        {
            return malformed();
        }
        value.updates_until = *until;
    }

    const auto replaces = detail::optional_string(*object, "replaces", max_id_length);
    const auto reset = detail::optional_string(*object, "clock_reset", max_id_length);
    if (!replaces || !reset || (!replaces->empty() && !is_upper_token(*replaces, max_id_length)) ||
        (!reset->empty() && !detail::is_token(*reset, max_id_length, "-")))
    {
        return malformed();
    }
    value.replaces = *replaces;
    value.clock_reset = *reset;
    return value;
}

Result<std::string> write_payload(const Licence& value)
{
    std::string licensee = "{";
    append_member(licensee, "name", detail::quote(value.licensee_name));
    if (!value.licensee_contact.empty())
    {
        licensee.push_back(',');
        append_member(licensee, "contact", detail::quote(value.licensee_contact));
    }
    if (!value.licensee_email.empty())
    {
        licensee.push_back(',');
        append_member(licensee, "email", detail::quote(value.licensee_email));
    }
    licensee.push_back('}');

    std::string features;
    append_list(features, value.features);
    std::string machines;
    append_list(machines, value.machines);

    std::string text = "{";
    append_member(text, "product", detail::quote(value.product));
    text.push_back(',');
    append_member(text, "licence_id", detail::quote(value.licence_id));
    text.push_back(',');
    append_member(text, "kind", detail::quote(kind_name(value.kind)));
    text.push_back(',');
    append_member(text, "edition", detail::quote(value.edition == Edition::pro ? "pro" : "enterprise"));
    text.push_back(',');
    append_member(text, "licensee", licensee);
    text.push_back(',');
    append_member(text, "issued_at", detail::quote(format_time(value.issued_at)));
    text.push_back(',');
    append_member(text, "starts_at", detail::quote(format_time(value.starts_at)));
    text.push_back(',');
    append_member(text, "expires_at", detail::quote(format_time(value.expires_at)));
    text.push_back(',');
    append_member(text, "grace_days", std::to_string(value.grace_days));
    text.push_back(',');
    append_member(text, "seats", std::to_string(value.seats));
    text.push_back(',');
    append_member(text, "features", features);
    text.push_back(',');
    append_member(text, "machines", machines);
    if (value.updates_until)
    {
        text.push_back(',');
        append_member(text, "updates_until", detail::quote(format_time(*value.updates_until)));
    }
    if (!value.replaces.empty())
    {
        text.push_back(',');
        append_member(text, "replaces", detail::quote(value.replaces));
    }
    if (!value.clock_reset.empty())
    {
        text.push_back(',');
        append_member(text, "clock_reset", detail::quote(value.clock_reset));
    }
    text += ",\"critical\":[]}";

    const auto read_back = parse_payload(as_bytes(text));
    if (!read_back || *read_back != value)
    {
        return malformed();
    }
    return text;
}

Result<RevocationList> parse_revocations(std::span<const std::uint8_t> payload)
{
    const auto object = payload_object(payload);
    if (!object)
    {
        return malformed();
    }
    if (const auto critical = check_critical(*object, known_revocation_fields); !critical)
    {
        return std::unexpected(critical.error());
    }

    RevocationList list;
    const auto sequence = detail::required_integer(*object, "sequence", 0, std::numeric_limits<std::int64_t>::max());
    const auto issued = required_time(*object, "issued_at");
    auto ids = detail::string_list(*object, "licences", max_revoked, max_id_length);
    if (!sequence || !issued || !ids ||
        !std::ranges::all_of(*ids, [](const std::string& id) { return is_upper_token(id, max_id_length); }))
    {
        return malformed();
    }
    list.sequence = static_cast<std::uint64_t>(*sequence);
    list.issued_at = *issued;
    list.licence_ids = std::move(*ids);

    if (const detail::JsonValue* keys_member = detail::find(*object, "keys"); keys_member != nullptr)
    {
        const detail::JsonArray* keys = keys_member->as_array();
        if (keys == nullptr || keys->size() > max_revoked)
        {
            return malformed();
        }
        for (const detail::JsonValue& entry : *keys)
        {
            const detail::JsonObject* member = entry.as_object();
            if (member == nullptr)
            {
                return malformed();
            }
            const auto key_id = detail::required_string(*member, "key_id", max_id_length);
            const auto from = detail::optional_string(*member, "from", time_text_length);
            if (!key_id || !from || !detail::is_token(*key_id, max_id_length, "-"))
            {
                return malformed();
            }
            RevokedKey key;
            key.key_id = *key_id;
            if (!from->empty())
            {
                const auto time = parse_time(*from);
                if (!time)
                {
                    return malformed();
                }
                key.from = *time;
            }
            list.keys.push_back(std::move(key));
        }
    }
    return list;
}

Result<std::string> write_revocations(const RevocationList& value)
{
    std::string keys = "[";
    bool first = true;
    for (const RevokedKey& key : value.keys)
    {
        if (!first)
        {
            keys.push_back(',');
        }
        first = false;
        keys.push_back('{');
        append_member(keys, "key_id", detail::quote(key.key_id));
        if (key.from)
        {
            keys.push_back(',');
            append_member(keys, "from", detail::quote(format_time(*key.from)));
        }
        keys.push_back('}');
    }
    keys.push_back(']');

    std::string ids;
    append_list(ids, value.licence_ids);

    std::string text = "{";
    append_member(text, "sequence", std::to_string(value.sequence));
    text.push_back(',');
    append_member(text, "issued_at", detail::quote(format_time(value.issued_at)));
    text.push_back(',');
    append_member(text, "licences", ids);
    text.push_back(',');
    append_member(text, "keys", keys);
    text += ",\"critical\":[]}";

    const auto read_back = parse_revocations(as_bytes(text));
    if (!read_back || *read_back != value)
    {
        return malformed();
    }
    return text;
}

Result<RevocationList> read_revocations(std::string_view file_text, std::span<const signed_file::TrustedKey> keys)
{
    const auto file = signed_file::parse(file_text);
    if (!file)
    {
        return std::unexpected(file.error());
    }
    const auto key = signed_file::verify(*file, revocations_purpose, keys);
    if (!key)
    {
        return std::unexpected(key.error());
    }
    auto list = parse_revocations(file->payload);
    if (!list)
    {
        return list;
    }
    if (const auto window = signed_file::check_key_window(**key, list->issued_at); !window)
    {
        return std::unexpected(window.error());
    }
    return list;
}

RevocationList newer(const RevocationList& current, const RevocationList& candidate)
{
    return candidate.sequence > current.sequence ? candidate : current;
}

bool Status::active() const noexcept
{
    return state == State::valid || state == State::expiring || state == State::grace;
}

Status no_licence()
{
    return {};
}

Status evaluate(std::string_view file_text, const Context& where)
{
    const auto file = signed_file::parse(file_text);
    if (!file)
    {
        return invalid(reason_for(file.error()));
    }
    const auto key = signed_file::verify(*file, licence_purpose, where.keys);
    if (!key)
    {
        return invalid(reason_for(key.error()));
    }
    const auto value = parse_payload(file->payload);
    if (!value)
    {
        return invalid(reason_for(value.error()));
    }
    return evaluate(*value, *key, where);
}

Status evaluate(const Licence& value, const signed_file::TrustedKey* signed_by, const Context& where)
{
    Status status;
    status.value = value;
    status.grace_ends = value.expires_at + days{value.grace_days};
    status.state = State::invalid;
    if (signed_by != nullptr)
    {
        status.key_id = signed_by->key_id;
    }
    if (value.product != where.product)
    {
        status.reason = Reason::wrong_product;
        return status;
    }
    if (signed_by != nullptr && !signed_file::check_key_window(*signed_by, value.issued_at))
    {
        status.reason = Reason::revoked;
        return status;
    }
    if (where.revocations != nullptr && revoked(value, signed_by, *where.revocations))
    {
        status.reason = Reason::revoked;
        return status;
    }
    if (!value.machines.empty() && std::ranges::none_of(value.machines, [&](const std::string& entry)
                                                        { return machine_matches(entry, where.computer_name); }))
    {
        status.reason = Reason::wrong_computer;
        return status;
    }

    // Times that have certainly passed. The clock may not read well before the latest of them.
    const TimePoint passed = std::max({where.latest_seen, where.build_time, value.issued_at});
    if (where.now + clock_tolerance < passed)
    {
        status.state = State::clock_suspect;
        status.reason = Reason::clock_suspect;
        return status;
    }
    if (where.now < value.starts_at)
    {
        status.state = State::not_yet_valid;
        status.reason = Reason::not_yet_valid;
        return status;
    }
    if (where.now <= value.expires_at)
    {
        status.days_left = whole_days(where.now, value.expires_at);
        const int threshold = value.kind == LicenceKind::trial ? expiring_days_trial : expiring_days;
        status.state = status.days_left <= threshold ? State::expiring : State::valid;
        status.reason = Reason::ok;
        return status;
    }
    if (where.now <= status.grace_ends)
    {
        status.days_left = whole_days(where.now, status.grace_ends);
        status.state = State::grace;
        status.reason = Reason::ok;
        return status;
    }
    status.state = State::expired;
    status.reason = Reason::expired;
    return status;
}

Status best(std::span<const Status> candidates)
{
    const auto rank = [](const Status& s)
    {
        return std::tuple{state_rank(s.state), s.value && s.value->edition == Edition::enterprise,
                          s.value ? s.value->expires_at : TimePoint::min()};
    };
    Status chosen = no_licence();
    for (const Status& candidate : candidates)
    {
        if (rank(candidate) > rank(chosen))
        {
            chosen = candidate;
        }
    }
    return chosen;
}

bool is_email_address(std::string_view text) noexcept
{
    if (text.empty() || text.size() > max_email_length)
    {
        return false;
    }
    const auto at = text.find('@');
    if (at == std::string_view::npos || at == 0 || text.find('@', at + 1) != std::string_view::npos)
    {
        return false;
    }
    // Before the @: letters, digits and the punctuation RFC 5322 allows without quoting. After it:
    // letters, digits, dots and hyphens.
    constexpr std::string_view local_marks = ".!#$%&'*+/=?^_`{|}~-";
    const auto is_alnum = [](char c)
    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
    const std::string_view local = text.substr(0, at);
    const std::string_view domain = text.substr(at + 1);
    if (!std::ranges::all_of(local,
                             [&](char c) { return is_alnum(c) || local_marks.find(c) != std::string_view::npos; }))
    {
        return false;
    }
    constexpr std::size_t shortest_domain = 3;
    if (domain.size() < shortest_domain || domain.find('.') == std::string_view::npos ||
        domain.find("..") != std::string_view::npos || !is_alnum(domain.front()) || !is_alnum(domain.back()))
    {
        return false;
    }
    return std::ranges::all_of(domain, [&](char c) { return is_alnum(c) || c == '.' || c == '-'; });
}

Entitlement check(const Status& current, std::string_view feature)
{
    Entitlement answer;
    answer.reason = current.reason;
    answer.days_left = current.days_left;
    if (current.value)
    {
        answer.licence_id = current.value->licence_id;
    }
    if (!current.active() || !current.value)
    {
        return answer;
    }
    answer.granted = std::ranges::any_of(current.value->features, [feature](const std::string& entry)
                                         { return feature_matches(entry, feature); });
    answer.reason = answer.granted ? Reason::ok : Reason::not_in_licence;
    return answer;
}

Entitlement check(const Status& current, std::string_view feature, TimePoint released)
{
    Entitlement answer = check(current, feature);
    if (answer.granted || current.state != State::expired || !current.value || !current.value->updates_until)
    {
        return answer;
    }
    // The licence has ended and nothing else is wrong with it. What was released within its window still
    // works, if the licence names it.
    const bool named = std::ranges::any_of(current.value->features, [feature](const std::string& entry)
                                           { return feature_matches(entry, feature); });
    if (named && released <= *current.value->updates_until)
    {
        answer.granted = true;
        answer.reason = Reason::ok;
        answer.by_version_window = true;
    }
    return answer;
}

bool feature_matches(std::string_view entry, std::string_view feature)
{
    if (entry.ends_with(".*"))
    {
        // "enterprise.*" covers "enterprise.x" and not "enterprise" or "enterprisex".
        entry.remove_suffix(1);
        return feature.size() > entry.size() && feature.starts_with(entry);
    }
    return entry == feature;
}

bool machine_matches(std::string_view entry, std::string_view computer_name)
{
    const bool prefix = entry.ends_with('*');
    if (prefix)
    {
        entry.remove_suffix(1);
    }
    if (computer_name.size() < entry.size() || (!prefix && computer_name.size() != entry.size()))
    {
        return false;
    }
    return std::ranges::equal(entry, computer_name.substr(0, entry.size()),
                              [](char a, char b) { return ascii_lower(a) == ascii_lower(b); });
}

ClockState observe(ClockState stored, TimePoint now)
{
    stored.latest_seen = std::max(stored.latest_seen, now);
    return stored;
}

ClockState apply_reset(ClockState stored, const Licence& imported)
{
    if (imported.clock_reset.empty() ||
        std::ranges::find(stored.applied_resets, imported.clock_reset) != stored.applied_resets.end())
    {
        return stored;
    }
    stored.latest_seen = imported.issued_at;
    stored.applied_resets.push_back(imported.clock_reset);
    if (stored.applied_resets.size() > max_applied_resets)
    {
        stored.applied_resets.erase(stored.applied_resets.begin());
    }
    return stored;
}

} // namespace adl::licence
