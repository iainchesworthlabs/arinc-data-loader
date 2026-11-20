// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/licence_files.hpp"

#include "adl/licence_platform/stamp.hpp"

#include <optional>
#include <utility>

namespace adl::load
{

LicenceSource licence_from_files(std::string settings_directory,
                                 std::vector<adl::licence::signed_file::TrustedKey> keys)
{
    return LicenceSource{[settings = std::move(settings_directory), trusted = std::move(keys)](
                             adl::platform::Host& host) -> std::optional<adl::loader_core::RecordedLicence>
                         {
                             const auto stamp = adl::licence_platform::stamp_of(host, settings, trusted);
                             if (!stamp)
                             {
                                 return std::nullopt; // no wall clock, so no licence can be judged
                             }
                             return adl::loader_core::RecordedLicence{stamp->edition, stamp->state, stamp->number};
                         }};
}

} // namespace adl::load
