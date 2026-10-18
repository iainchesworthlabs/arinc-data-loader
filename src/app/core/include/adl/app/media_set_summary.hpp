#pragma once

#include <QString>

#include "adl/app/events.hpp"

namespace adl::app {

// Reads the media set under `path` (a directory with one directory per member, as
// adl::media_tree writes them) and summarises it. Blocks while it reads, so it belongs on the
// worker's thread. A set that cannot be read comes back with `error` set.
[[nodiscard]] MediaSetSummary read_media_set_summary(const QString& path);

}  // namespace adl::app
