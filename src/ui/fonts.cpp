#include "adl/ui/fonts.hpp"

#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QString>

namespace adl::ui {

void register_fonts() {
    // Archivo is SIL OFL 1.1; fonts/OFL.txt ships beside the faces. Static weights rather than
    // the variable font, because Qt's font database resolves named static weights on every
    // platform this builds for.
    for (const char* face :
         {":/adl/fonts/Archivo-Regular.ttf", ":/adl/fonts/Archivo-Medium.ttf",
          ":/adl/fonts/Archivo-SemiBold.ttf", ":/adl/fonts/Archivo-ExtraBold.ttf"}) {
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(face)) < 0) {
            qWarning("could not register bundled font %s", face);
        }
    }

    QFont default_font = QGuiApplication::font();
    default_font.setFamily(QStringLiteral("Archivo"));
    QGuiApplication::setFont(default_font);
}

}  // namespace adl::ui
