// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "report.hpp"

#include "messages.hpp"

#include <algorithm>
#include <ostream>
#include <vector>

namespace adl::licence_owner
{

namespace lic = adl::licence;
namespace sf = adl::licence::signed_file;

namespace
{

[[nodiscard]] std::string join(const std::vector<std::string>& items, std::string_view empty)
{
    if (items.empty())
    {
        return std::string(empty);
    }
    std::string out;
    for (const std::string& item : items)
    {
        if (!out.empty())
        {
            out += ", ";
        }
        out += item;
    }
    return out;
}

[[nodiscard]] std::string_view kind_word(lic::LicenceKind kind) noexcept
{
    switch (kind)
    {
    case lic::LicenceKind::trial:
        return "trial";
    case lic::LicenceKind::annual:
        return "annual";
    case lic::LicenceKind::enterprise:
        return "enterprise";
    }
    return "annual";
}

} // namespace

SignatureCheck check_signature(const sf::Envelope& file, std::string_view purpose, std::span<const sf::TrustedKey> keys,
                               const lic::TimePoint* issued_at)
{
    SignatureCheck result;
    const auto verified = sf::verify(file, purpose, keys);
    if (!verified)
    {
        if (verified.error() == lic::Error::unknown_key)
        {
            result.kind = SignatureCheck::Kind::no_key;
        }
        else
        {
            result.kind = SignatureCheck::Kind::does_not_verify;
            result.detail = std::string(lic::to_string(verified.error()));
        }
        return result;
    }
    if (issued_at != nullptr)
    {
        if (const auto window = sf::check_key_window(**verified, *issued_at); !window)
        {
            result.kind = SignatureCheck::Kind::does_not_verify;
            result.detail = std::string(lic::to_string(window.error()));
            return result;
        }
    }
    result.kind = SignatureCheck::Kind::verifies;
    result.key = **verified;
    return result;
}

bool trusted_by_this_build(const sf::TrustedKey& key)
{
    const std::vector<sf::TrustedKey> own = sf::product_trust_list();
    return std::ranges::any_of(
        own, [&key](const sf::TrustedKey& entry)
        { return entry.key_id == key.key_id && entry.purpose == key.purpose && entry.key == key.key; });
}

std::string_view reason_name(lic::Reason value) noexcept
{
    switch (value)
    {
    case lic::Reason::ok:
        return "ok";
    case lic::Reason::no_licence:
        return "no_licence";
    case lic::Reason::expired:
        return "expired";
    case lic::Reason::not_yet_valid:
        return "not_yet_valid";
    case lic::Reason::clock_suspect:
        return "clock_suspect";
    case lic::Reason::malformed:
        return "malformed";
    case lic::Reason::newer_version:
        return "newer_version";
    case lic::Reason::bad_signature:
        return "bad_signature";
    case lic::Reason::revoked:
        return "revoked";
    case lic::Reason::wrong_computer:
        return "wrong_computer";
    case lic::Reason::wrong_product:
        return "wrong_product";
    case lic::Reason::not_in_licence:
        return "not_in_licence";
    }
    return "malformed";
}

void print_signature(std::ostream& out, const SignatureCheck& check)
{
    switch (check.kind)
    {
    case SignatureCheck::Kind::verifies:
        out << "Signature: " << message::signature_verifies << '\n';
        out << "Key trusted by this build: " << (trusted_by_this_build(check.key) ? "yes" : "no") << '\n';
        break;
    case SignatureCheck::Kind::does_not_verify:
        out << "Signature: " << format(message::signature_fails, {check.detail}) << '\n';
        break;
    case SignatureCheck::Kind::no_key:
        out << "Signature: " << message::signature_not_checked << '\n';
        break;
    }
}

void print_licence(std::ostream& out, const lic::Licence& value)
{
    out << "Licence: " << value.licence_id << '\n';
    out << "Product: " << value.product << '\n';
    out << "Kind: " << kind_word(value.kind) << '\n';
    out << "Edition: " << (value.edition == lic::Edition::pro ? "pro" : "enterprise") << '\n';
    out << "Licensee: " << value.licensee_name << '\n';
    if (!value.licensee_contact.empty())
    {
        out << "Contact: " << value.licensee_contact << '\n';
    }
    if (!value.licensee_email.empty())
    {
        out << "Email: " << value.licensee_email << '\n';
    }
    out << "Issued: " << lic::format_time(value.issued_at) << '\n';
    out << "Starts: " << lic::format_time(value.starts_at) << '\n';
    out << "Expires: " << lic::format_time(value.expires_at) << '\n';
    out << "Grace days: " << value.grace_days << '\n';
    out << "Seats: " << value.seats << (value.seats == 0 ? " (site licence)" : "") << '\n';
    out << "Features: " << join(value.features, "none") << '\n';
    out << "Computers: " << join(value.machines, "any") << '\n';
    if (value.updates_until)
    {
        out << "Updates until: " << lic::format_time(*value.updates_until) << '\n';
    }
    if (!value.replaces.empty())
    {
        out << "Replaces: " << value.replaces << '\n';
    }
    if (!value.clock_reset.empty())
    {
        out << "Clock reset: " << value.clock_reset << '\n';
    }
}

void print_revocations(std::ostream& out, const lic::RevocationList& value)
{
    out << "Sequence: " << value.sequence << '\n';
    out << "Issued: " << lic::format_time(value.issued_at) << '\n';
    out << "Licences withdrawn: " << value.licence_ids.size() << '\n';
    for (const std::string& id : value.licence_ids)
    {
        out << "Withdrawn licence: " << id << '\n';
    }
    out << "Keys withdrawn: " << value.keys.size() << '\n';
    for (const lic::RevokedKey& key : value.keys)
    {
        out << "Withdrawn key: " << key.key_id;
        if (key.from)
        {
            out << " from " << lic::format_time(*key.from);
        }
        out << '\n';
    }
}

} // namespace adl::licence_owner
