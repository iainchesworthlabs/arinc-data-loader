// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The inspection document of `adl_make_set inspect --json`: one JSON object on one line that says everything
// a comparison, a manifest or a site rule needs about a media set (docs/file-reference.md, "The media set
// inspection"). It is not an event: it has its own `format` and `kind`, and carries the outcome itself, so a
// failure is a document too.

#include "adl/make_set/command.hpp"
#include "adl/set_maker/inspect.hpp"

#include <string>
#include <string_view>

namespace adl::make_set
{

// The document, without its line feed. `inspection` is null when the outcome is a failure; the lists are
// then empty and `media_set` is null.
[[nodiscard]] std::string inspection_document(const Verdict& verdict, std::string_view folder,
                                              const adl::set_maker::Inspection* inspection);

} // namespace adl::make_set
