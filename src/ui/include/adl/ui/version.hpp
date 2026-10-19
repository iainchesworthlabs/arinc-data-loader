#pragma once

#include <QString>

namespace adl::ui {

// The version and provenance block the About dialogs show: version, git describe, full commit,
// branch, build target and Qt version, with a dirty marker when the tree had uncommitted tracked
// changes when it was configured. The application name is the first line's subject.
QString version_details(const QString& application_name);

}  // namespace adl::ui
