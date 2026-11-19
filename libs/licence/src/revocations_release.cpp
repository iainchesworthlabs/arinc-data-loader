// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence/licence.hpp"

// The revocation list this release carries. It holds licence numbers and key ids only, never names.
//
// The list is empty and has sequence 0, so any signed list a customer is sent is newer. When a licence or
// a key is withdrawn, the owner adds its id here and raises the sequence number to that of the signed list
// of the same day, so that the release and the file say the same thing.

namespace adl::licence
{

RevocationList builtin_revocations()
{
    return {};
}

} // namespace adl::licence
