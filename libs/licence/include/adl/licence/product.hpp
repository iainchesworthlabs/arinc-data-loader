// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// What is this product's own in the licence library, and nothing else: its name in a licence file, how its
// licence numbers begin and the feature names it asks about. Everything else in adl::licence takes no
// product name, so that the library can move to a repository both products share. The trust list
// (trusted_keys.cpp) and the licences withdrawn with this release (revocations_release.cpp) are the
// other two files that belong to the product.

#include <string_view>

namespace adl::licence
{

// The `product` a licence file for this product carries. A licence for another product is refused.
inline constexpr std::string_view product_id = "adl";

// A licence number is this, the year and six digits: ADL-2026-000001. It is the owner's tool that writes
// it. The reader does not look at its shape: it takes any licence number of upper-case letters, digits and
// hyphens, 64 characters at most, so that the library can be shared between products. What ties a licence to
// this product is the signed `product` field above, not the number.
inline constexpr std::string_view licence_id_prefix = "ADL-";

// What a licence can carry. The edition alone grants nothing: a licence names the features it turns on.
// A name is lower case letters, digits, '.', '-' and '_'; a licence may also carry a prefix that ends in
// ".*" to cover every name below it.
inline constexpr std::string_view feature_support = "support";
inline constexpr std::string_view feature_data_packs = "data_packs";

} // namespace adl::licence
