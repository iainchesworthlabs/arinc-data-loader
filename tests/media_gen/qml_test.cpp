// Loads the generator's window (argv[1], the path of GeneratorWindow.qml) with the real model,
// offscreen, fills the form, starts a generation through the window's own Generate button and
// fails if the window does not load, the generation does not end with a verified media set or the
// QML engine has a single complaint. With a second argument it saves what the window looks like
// there. The platform's graphics are not used: the test's environment asks for the offscreen
// platform and the software renderer.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QPointF>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include "adl/gen/generator_model.hpp"
#include "adl/gen/source_file_model.hpp"
#include "adl/ui/fonts.hpp"
#include "adl/ui/language_manager.hpp"
#include "temp_dir.hpp"

namespace {

// Lets the window and the model's signals run until the job is done (or 30 seconds pass).
bool wait_until_idle(adl::gen::GeneratorModel& model) {
    QEventLoop loop;
    QTimer poll;
    QTimer limit;
    limit.setSingleShot(true);
    bool timed_out = false;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (!model.busy()) {
            loop.quit();
        }
    });
    QObject::connect(&limit, &QTimer::timeout, &loop, [&] {
        timed_out = true;
        loop.quit();
    });
    poll.start(10);
    limit.start(30000);
    loop.exec();
    return !timed_out;
}

// The item called `name` in the visual tree below `root` (a delegate's items are not always
// children of anything a QObject search would find).
QQuickItem* find_item(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = find_item(child, name)) {
            return found;
        }
    }
    return nullptr;
}

QUrl url_of(const std::filesystem::path& path) {
    return QUrl::fromLocalFile(QString::fromStdU16String(path.u16string()));
}

}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    if (argc > 3) {
        std::fprintf(stderr, "usage: adl_gen_qml_test [picture.png [right-to-left.png]]\n");
        return 2;
    }
    adl::ui::register_fonts();
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // The files the generation reads, and the directory it writes below.
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const std::filesystem::path data_path = sources.path() / "DATA.LUP";
    {
        std::ofstream out(data_path, std::ios::binary);
        const std::string_view text = "DATA DATA DATA!!";
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
    const std::filesystem::path support_path = sources.path() / "CONFIG.LDR";
    {
        std::ofstream out(support_path, std::ios::binary);
        const std::string_view text = "CONFIG 12345";
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    adl::gen::GeneratorModel model;
    model.setLoadPartNumber(QStringLiteral("ACM47-1234-5678"));
    model.setMediaSetPartNumber(QStringLiteral("ACM-SET-0001"));
    model.setTargetHardwareIds({QStringLiteral("ACME")});
    model.chooseOutputDirectory(url_of(output.path()));
    model.addFiles({url_of(data_path), url_of(support_path)});
    // CONFIG.LDR is the classic support file, not a data file.
    auto* files = static_cast<adl::gen::SourceFileModel*>(model.files());
    files->setData(files->index(1), QStringLiteral("support"), adl::gen::SourceFileModel::KindRole);

    QQmlApplicationEngine engine;
    int complaints = 0;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [&complaints](const QList<QQmlError>& warnings) {
                         for (const QQmlError& warning : warnings) {
                             std::fprintf(stderr, "QML: %s\n", qPrintable(warning.toString()));
                             ++complaints;
                         }
                     });
    // The picker in the header reads the language manager, exactly as the application's does.
    adl::ui::LanguageManager language_manager(application, engine, QStringLiteral("adl_media_gen"));
    language_manager.setLanguage(QStringLiteral("fr"));  // English by way of another: a change
    language_manager.setLanguage(QStringLiteral("en"));
    QObject::connect(&language_manager, &adl::ui::LanguageManager::currentLanguageChanged, &model,
                     &adl::gen::GeneratorModel::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("languageManager"), &language_manager);
    engine.setInitialProperties(
        {{QStringLiteral("app"), QVariant::fromValue(&model)},
         // ADL_DARK=1 renders in the dark scheme (the window sets a dark palette, which is what
         // a dark desktop theme supplies), so both schemes can be checked and pictured.
         {QStringLiteral("renderDark"),
          QVariant::fromValue(qEnvironmentVariable("ADL_DARK") == QStringLiteral("1"))}});
    engine.loadFromModule("ArincMediaGen", "GeneratorWindow");

    if (engine.rootObjects().size() != 1) {
        std::fprintf(stderr, "the window did not load\n");
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (window == nullptr || window->title() != QStringLiteral("ARINC Media Set Generator")) {
        std::fprintf(stderr, "the root object is not the application window\n");
        return 1;
    }

    // The form the model holds should be enough for a generation.
    if (!model.canGenerate()) {
        std::fprintf(stderr, "the form is not ready: %s\n", qPrintable(model.filesError()));
        return 1;
    }
    QQuickItem* button = find_item(window->contentItem(), QStringLiteral("generateButton"));
    if (button == nullptr) {
        std::fprintf(stderr, "the Generate button is not in the window\n");
        return 1;
    }
    if (!button->isEnabled()) {
        std::fprintf(stderr, "the Generate button is disabled\n");
        return 1;
    }

    // The state is said once, in the header, and a complete form says READY.
    QQuickItem* status = find_item(window->contentItem(), QStringLiteral("statusChip"));
    QQuickItem* attempt = find_item(window->contentItem(), QStringLiteral("attemptBanner"));
    QQuickItem* result = find_item(window->contentItem(), QStringLiteral("resultBanner"));
    if (status == nullptr || attempt == nullptr || result == nullptr) {
        std::fprintf(stderr, "the status chip or a banner is not in the window\n");
        return 1;
    }
    const auto status_text = [&] { return status->property("text").toString(); };
    if (status_text() != QStringLiteral("READY") || attempt->isVisible() || result->isVisible()) {
        std::fprintf(stderr, "a complete form does not read as READY: %s\n",
                     qPrintable(status_text()));
        return 1;
    }

    // A name the standard allows but the folder tree refuses (a `*`) is marked on its row, with
    // the rule, and the form is not ready; the right name makes it ready again.
    files->setData(files->index(0), QStringLiteral("DATA*.LUP"),
                   adl::gen::SourceFileModel::FileNameRole);
    QCoreApplication::processEvents();
    QQuickItem* row_message = find_item(window->contentItem(), QStringLiteral("rowMessage"));
    if (row_message == nullptr || !row_message->isVisible() ||
        !row_message->property("text").toString().contains(QStringLiteral("every system")) ||
        model.canGenerate() || status_text() != QStringLiteral("INCOMPLETE")) {
        std::fprintf(stderr, "a name the folder tree refuses is not marked on its row: %s\n",
                     row_message == nullptr ? "no row message"
                                            : qPrintable(row_message->property("text").toString()));
        return 1;
    }
    files->setData(files->index(0), QStringLiteral("DATA.LUP"),
                   adl::gen::SourceFileModel::FileNameRole);
    QCoreApplication::processEvents();
    if (!model.canGenerate() || row_message->isVisible()) {
        std::fprintf(stderr, "the row kept its message after the name was put right\n");
        return 1;
    }

    // The NO CHECK VALUES chip counts the table's Check value column too: with all three pickers
    // at none it shows, and a check value on any file takes it away.
    QQuickItem* no_check = find_item(window->contentItem(), QStringLiteral("noCheckValuesChip"));
    if (no_check == nullptr || !no_check->isVisible()) {
        std::fprintf(stderr, "the NO CHECK VALUES chip is missing with no check value anywhere\n");
        return 1;
    }
    files->setData(files->index(0), 3, adl::gen::SourceFileModel::CheckTypeRole);
    QCoreApplication::processEvents();
    if (no_check->isVisible()) {
        std::fprintf(stderr, "the NO CHECK VALUES chip ignores the Check value column\n");
        return 1;
    }
    files->setData(files->index(0), 0, adl::gen::SourceFileModel::CheckTypeRole);
    QCoreApplication::processEvents();
    if (!no_check->isVisible()) {
        std::fprintf(stderr, "the NO CHECK VALUES chip did not come back\n");
        return 1;
    }

    // The footer's state mark is a ToneGlyph, as the loader's is: a dot while nothing runs.
    QQuickItem* mark = find_item(window->contentItem(), QStringLiteral("statusMark"));
    if (mark == nullptr || mark->property("glyph").toString() != QStringLiteral("dot")) {
        std::fprintf(stderr, "the footer's state mark is not a dot while nothing runs\n");
        return 1;
    }

    // An incomplete form is INCOMPLETE and shows no complaint until Generate is pressed on it;
    // then the banner says what is missing, and nothing is written.
    model.setLoadPartNumber(QString());
    QCoreApplication::processEvents();
    if (status_text() != QStringLiteral("INCOMPLETE") || attempt->isVisible()) {
        std::fprintf(stderr, "an incomplete form is alarming before an attempt: %s\n",
                     qPrintable(status_text()));
        return 1;
    }
    QMetaObject::invokeMethod(button, "click");
    QCoreApplication::processEvents();
    if (!attempt->isVisible() || model.resultState() != 0 || result->isVisible()) {
        std::fprintf(stderr, "Generate on an incomplete form did not say what is missing\n");
        return 1;
    }
    model.setLoadPartNumber(QStringLiteral("ACM47-1234-5678"));
    QCoreApplication::processEvents();
    if (!model.canGenerate() || attempt->isVisible()) {
        std::fprintf(stderr, "the banner stayed after the form was completed\n");
        return 1;
    }

    // The window's own button starts the generation, exactly as an operator would.
    QMetaObject::invokeMethod(button, "click");
    QCoreApplication::processEvents();
    if (!wait_until_idle(model)) {
        std::fprintf(stderr, "the generation did not end\n");
        return 1;
    }
    QCoreApplication::processEvents();

    if (model.resultState() != 1 || model.outputPath().isEmpty()) {
        std::fprintf(stderr, "the generation did not complete: %s\n",
                     qPrintable(model.resultText()));
        return 1;
    }

    if (status_text() != QStringLiteral("GENERATED") || !result->isVisible() ||
        attempt->isVisible()) {
        std::fprintf(stderr, "the window does not show the outcome once: %s\n",
                     qPrintable(status_text()));
        return 1;
    }

    const QImage picture = window->grabWindow();
    if (picture.isNull()) {
        std::fprintf(stderr, "the window could not be drawn\n");
        return 1;
    }
    if (argc > 1 && !picture.save(QString::fromLocal8Bit(argv[1]))) {
        std::fprintf(stderr, "the picture could not be saved\n");
        return 1;
    }

    // The language picker is in the header and the header mirrors with the language: in Hebrew
    // the picker is on the left of the window, in English on the right. A second argument keeps
    // a picture of the mirrored window.
    QQuickItem* picker = find_item(window->contentItem(), QStringLiteral("languagePicker"));
    if (picker == nullptr) {
        std::fprintf(stderr, "the window has no language picker\n");
        return 1;
    }
    const auto picker_centre = [&] {
        return picker->mapToScene(QPointF{picker->width() / 2, 0}).x();
    };
    if (picker_centre() < window->width() / 2.0) {
        std::fprintf(stderr, "the language picker is not on the right in English\n");
        return 1;
    }
    for (const QString& code : {QStringLiteral("he"), QStringLiteral("yi")}) {
        if (!language_manager.setLanguage(code)) {
            std::fprintf(stderr, "the language %s is not accepted\n", qPrintable(code));
            return 1;
        }
        QCoreApplication::processEvents();
        if (picker_centre() > window->width() / 2.0) {
            std::fprintf(stderr, "the header did not mirror in %s\n", qPrintable(code));
            return 1;
        }
        // Changing the language is not an edit of the form: the outcome of the run, and the
        // Open folder button that goes with it, stay.
        if (model.resultState() != 1 || model.outputPath().isEmpty() || !result->isVisible()) {
            std::fprintf(stderr, "the language %s cleared the outcome of the run\n",
                         qPrintable(code));
            return 1;
        }
        if (code == QStringLiteral("he") && argc > 2) {
            const QImage mirrored = window->grabWindow();
            if (mirrored.isNull() || !mirrored.save(QString::fromLocal8Bit(argv[2]))) {
                std::fprintf(stderr, "the right-to-left picture could not be saved\n");
                return 1;
            }
        }
    }
    language_manager.setLanguage(QStringLiteral("en"));
    QCoreApplication::processEvents();
    if (picker_centre() < window->width() / 2.0) {
        std::fprintf(stderr, "English did not put the header back\n");
        return 1;
    }

    if (complaints != 0) {
        std::fprintf(stderr, "%d QML complaint(s)\n", complaints);
        return 1;
    }
    return 0;
}
