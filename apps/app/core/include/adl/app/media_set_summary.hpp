// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Reading a media set from a directory into the summary the window shows
// (MediaSetSummary, events.hpp), and making the index of its files that an
// upload reads them through (media_source.hpp).

#include "adl/app/events.hpp"
#include "adl/app/media_source.hpp"

#include <QString>
#include <memory>

namespace adl::app
{

// Reads the media set under `path` (a directory with one directory per member, as
// adl::media_tree writes them) and summarises it. Blocks while it reads, so it belongs on the
// worker's thread. A set that cannot be read comes back with `error` set.
// With `index`, also makes the index of the set's files that an upload reads them through; it is
// left empty when the set cannot be read.
[[nodiscard]] MediaSetSummary read_media_set_summary(const QString& path,
                                                     std::shared_ptr<const MediaSetIndex>* index = nullptr);

} // namespace adl::app
