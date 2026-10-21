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
    //
    // Archivo has no Hebrew letters, so the windows in Hebrew and Yiddish are set in Noto Sans
    // Hebrew (also SIL OFL 1.1, fonts/NotoSansHebrew-OFL.txt), cut as static instances of the
    // weights the design uses from Google's variable font; LanguageManager makes it the
    // application's typeface for those two languages, and Theme.headingFamily follows. There is no
    // Chinese or Japanese face here: those scripts are drawn in the system's own CJK fonts.
    for (const char* face :
         {":/adl/fonts/Archivo-Regular.ttf", ":/adl/fonts/Archivo-Medium.ttf",
          ":/adl/fonts/Archivo-SemiBold.ttf", ":/adl/fonts/Archivo-ExtraBold.ttf",
          ":/adl/fonts/NotoSansHebrew-Regular.ttf", ":/adl/fonts/NotoSansHebrew-Medium.ttf",
          ":/adl/fonts/NotoSansHebrew-SemiBold.ttf", ":/adl/fonts/NotoSansHebrew-ExtraBold.ttf"}) {
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(face)) < 0) {
            qWarning("could not register bundled font %s", face);
        }
    }

    QFont default_font = QGuiApplication::font();
    default_font.setFamily(QStringLiteral("Archivo"));
    QGuiApplication::setFont(default_font);
}

}  // namespace adl::ui
