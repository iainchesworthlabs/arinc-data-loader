#pragma once

#include <memory>

#include <QString>

#include "adl/app/events.hpp"
#include "adl/app/media_source.hpp"

namespace adl::app {

// Reads the media set under `path` (a directory with one directory per member, as
// adl::media_tree writes them) and summarises it. Blocks while it reads, so it belongs on the
// worker's thread. A set that cannot be read comes back with `error` set.
// With `index`, also makes the index of the set's files that an upload reads them through; it is
// left empty when the set cannot be read.
[[nodiscard]] MediaSetSummary read_media_set_summary(
    const QString& path, std::shared_ptr<const MediaSetIndex>* index = nullptr);

}  // namespace adl::app
