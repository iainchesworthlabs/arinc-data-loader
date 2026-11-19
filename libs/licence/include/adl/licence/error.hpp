// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Why a signed file or a licence payload was refused, and the Result alias the readers return.
// Failures are returned, never thrown, and nothing here needs an exception or RTTI.

#include <expected>
#include <string_view>

namespace adl::licence
{

enum class Error
{
    too_large,     // larger than signed_file::max_file_size
    malformed,     // not the envelope or payload this version writes: bad JSON, a duplicate key, a
                   // missing or mistyped field, bad base64url, a bad date
    newer_version, // an envelope format, or a field named in `critical`, that this version does not
                   // know: the file was made for a newer version
    wrong_purpose, // the envelope's purpose is not the one the caller asked for
    unknown_key,   // no trusted key has the envelope's key id for this purpose
    key_not_valid, // the key is withdrawn, or the file was signed outside the key's window
    bad_signature, // the signature does not verify
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] constexpr std::string_view to_string(Error error) noexcept
{
    switch (error)
    {
    case Error::too_large:
        return "the file is too large";
    case Error::malformed:
        return "the file is damaged or is not a signed file";
    case Error::newer_version:
        return "the file was made for a newer version";
    case Error::wrong_purpose:
        return "the file is signed for a different purpose";
    case Error::unknown_key:
        return "the file is signed with a key this version does not know";
    case Error::key_not_valid:
        return "the signing key was withdrawn or was not valid then";
    case Error::bad_signature:
        return "the signature does not verify";
    }
    return "unknown licence error";
}

} // namespace adl::licence
