#include "adl/ui/version.hpp"

#include <QFile>
#include <QIODevice>
#include <QQmlContext>
#include <QString>
#include <QtGlobal>

#include "adl/ui/build_info.hpp"

namespace adl::ui {

QString version_details(const QString& application_name) {
    QString text =
        QStringLiteral(
            "%1 %2\n  release: %3\n  commit:  %4\n  branch:  %5\n  target:  %6\n  qt:      %7")
            .arg(application_name)
            .arg(QString::fromLatin1(kVersionFull))
            .arg(QString::fromLatin1(kGitDescribe))
            .arg(QString::fromLatin1(kGitCommitFull))
            .arg(QString::fromLatin1(kGitBranch))
            .arg(QString::fromLatin1(kBuildTarget))
            .arg(QString::fromLatin1(qVersion()));
    if (kGitDirty) {
        text += QStringLiteral("\n  state:   dirty (uncommitted changes)");
    }
    return text;
}

QString licence_notices() {
    QFile file(QStringLiteral(":/adl/notices/NOTICES.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

void set_about_properties(QQmlContext& context, const QString& application_name) {
    context.setContextProperty(QStringLiteral("appVersionDetails"),
                               version_details(application_name));
    context.setContextProperty(QStringLiteral("appLicenceNotices"), licence_notices());
}

}  // namespace adl::ui
