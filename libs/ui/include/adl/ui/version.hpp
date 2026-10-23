// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QQmlContext;

namespace adl::ui
{

// The version and provenance block the About dialogs show: version, git describe, full commit,
// branch, build target and Qt version, with a dirty marker when the tree had uncommitted tracked
// changes when it was configured. The application name is the first line's subject.
QString version_details(const QString& application_name);

// The third-party notices this build carries: the NOTICES.txt embedded as
// :/adl/notices/NOTICES.txt (cmake/AdlUi.cmake writes it, and places a copy beside the
// executables). Empty when the build embedded none.
QString licence_notices();

// What the About dialogs read, as context properties of the window's QML: appVersionDetails (the
// version block above) and appLicenceNotices (the text of the Licences dialog).
void set_about_properties(QQmlContext& context, const QString& application_name);

} // namespace adl::ui
