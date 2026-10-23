// Loads the application's window (the compiled ArincDataLoader QML module) with the real model,
// offscreen, drives the first-run screen, then runs a FIND and an Information operation through
// the model against a simulated target on the fake platform while the window is showing, then
// reads a media set, ticks its load and starts the upload through the window's own check box and
// button, and fails if the window does not load, the operations do not end as they should or the
// QML engine has a single complaint. With an argument it saves what the window looks like there.
// The platform's graphics are not used: the test's environment asks for the offscreen platform and
// the software renderer.

#include "adl/app/loader_model.hpp"
#include "adl/app/models.hpp"
#include "adl/app/wording.hpp"
#include "adl/ui/fonts.hpp"
#include "adl/ui/language_manager.hpp"
#include "adl/ui/version.hpp"
#include "bytes.hpp"
#include "frame_profiler.hpp"
#include "rig.hpp"
#include "scratch_settings.hpp"
#include "upload_fixture.hpp"

#include <adl/profiling.hpp>

#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{

// Lets the window and the model's signals run until the job is done (or 20 seconds pass).
bool wait_until_idle(adl::app::LoaderModel& model)
{
    QEventLoop loop;
    QTimer poll;
    QTimer limit;
    limit.setSingleShot(true);
    bool timed_out = false;
    QObject::connect(&poll, &QTimer::timeout, &loop,
                     [&]
                     {
                         if (!model.busy())
                         {
                             loop.quit();
                         }
                     });
    QObject::connect(&limit, &QTimer::timeout, &loop,
                     [&]
                     {
                         timed_out = true;
                         loop.quit();
                     });
    poll.start(10);
    limit.start(20000);
    loop.exec();
    return !timed_out;
}

// The item called `name` in the visual tree below `root` (a delegate's items are not always
// children of anything a QObject search would find).
QQuickItem* find_item(QQuickItem* root, const QString& name)
{
    if (root->objectName() == name)
    {
        return root;
    }
    for (QQuickItem* child : root->childItems())
    {
        if (QQuickItem* found = find_item(child, name))
        {
            return found;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    if (argc > 6)
    {
        std::fprintf(stderr, "usage: adl_app_qml_test [picture.png [first-run.png [rtl.png "
                             "[preferences.png [about.png]]]]]\n");
        return 2;
    }
    adl::ui::register_fonts();
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // The workload a profiling build captures: this process's GUI thread draws the window and
    // runs the operations the application's operator would.
    adl::profiling::thread_name("gui");
    adl::profiling::app_info("ARINC Data Loader (window test)");

    // The model reads QSettings without arguments; the test keeps that out of the operator's
    // real settings, and out of the run before this one: the first-run screen is passed by this
    // test, so a shared file would make the next run's window think it had been used before.
    // The directory is removed when the program ends.
    QCoreApplication::setOrganizationName(QStringLiteral("adl-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("adl_app_qml_test"));
    const adl::test::ScratchSettings scratch_settings;

    adl::test::UploadFixture fixture;
    adl::test::SimPlan plan = fixture.plan();
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    plan.files = {adl::test::DownloadItem{"A.BIN", adl::test::bytes_of("downloaded"), "a file"},
                  adl::test::DownloadItem{"B.BIN", adl::test::bytes_of("other"), ""}};
    adl::test::Rig rig(plan);
    if (!fixture.write(rig))
    {
        std::fprintf(stderr, "the media set could not be written\n");
        return 1;
    }
    adl::app::LoaderModel model(
        [&rig]() -> std::expected<adl::app::HostLease, QString>
        {
            adl::app::HostLease lease;
            lease.host = &rig.loader_host;
            return lease;
        });

    // Held in an optional so the engine can be destroyed explicitly at the end, before the
    // language manager (the window's bindings read it; the application's main.cpp does the same).
    std::optional<QQmlApplicationEngine> engine{std::in_place};
    // The window's Preferences read the language manager, exactly as the application's do: the
    // test registers a real one, so a binding that reads it cannot be the difference.
    adl::ui::LanguageManager language_manager(application, *engine, QStringLiteral("adl_app"));
    language_manager.applyInitialLanguage();
    // The window under test starts in English whatever this machine's locale is; the languages
    // it ships are switched through at the end, and English is put back.
    language_manager.setLanguage(QStringLiteral("en"));
    // What the model holds as phrases follows the language, as in the application's main().
    QObject::connect(&language_manager, &adl::ui::LanguageManager::currentLanguageChanged, &model,
                     &adl::app::LoaderModel::retranslate);
    engine->rootContext()->setContextProperty(QStringLiteral("languageManager"), &language_manager);
    adl::ui::set_about_properties(*engine->rootContext(), QStringLiteral("ARINC Data Loader"));

    int complaints = 0;
    QObject::connect(&*engine, &QQmlApplicationEngine::warnings,
                     [&complaints](const QList<QQmlError>& warnings)
                     {
                         for (const QQmlError& warning : warnings)
                         {
                             std::fprintf(stderr, "QML: %s\n", qPrintable(warning.toString()));
                             ++complaints;
                         }
                     });
    engine->setInitialProperties(
        {{QStringLiteral("app"), QVariant::fromValue(&model)},
         // ADL_DARK=1 renders in the dark scheme (the window sets a dark palette, which is what
         // a dark desktop theme supplies), so both schemes can be checked and pictured.
         {QStringLiteral("renderDark"), QVariant::fromValue(qEnvironmentVariable("ADL_DARK") == QStringLiteral("1"))}});
    {
        const adl::profiling::Zone zone{"app: load QML"};
        engine->loadFromModule("ArincDataLoader", "Main");
    }

    if (engine->rootObjects().size() != 1)
    {
        std::fprintf(stderr, "the window did not load\n");
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst());
    if (window == nullptr || window->title() != QStringLiteral("ARINC Data Loader"))
    {
        std::fprintf(stderr, "the root object is not the application window\n");
        return 1;
    }
    // The frame loop of the workload, for the trace; destroyed after the engine below.
    std::optional<adl::app::FrameProfiler> frame_profiler{std::in_place, window};

    // A window that has never been used opens on the first-run screen; passing it is what makes
    // the workbench appear, and it is the operator's first act, so it is the test's.
    QCoreApplication::processEvents();
    if (!model.firstRun())
    {
        std::fprintf(stderr, "a fresh settings file still reports the first run as done\n");
        return 1;
    }
    // With a second argument, what the first launch looks like is kept too: the same window
    // before the first-run screen has been passed.
    if (argc > 2)
    {
        const QImage first_run = window->grabWindow();
        if (first_run.isNull() || !first_run.save(QString::fromLocal8Bit(argv[2])))
        {
            std::fprintf(stderr, "the first-run picture could not be saved\n");
            return 1;
        }
    }
    QQuickItem* content = window->contentItem();
    QQuickItem* start = find_item(content, QStringLiteral("firstRunStart"));
    if (start == nullptr)
    {
        std::fprintf(stderr, "the first-run screen's Start button is not in the window\n");
        return 1;
    }
    QMetaObject::invokeMethod(start, "click");
    QCoreApplication::processEvents();
    if (model.firstRun())
    {
        std::fprintf(stderr, "passing the first-run screen did not let the workbench through\n");
        return 1;
    }

    // The header's Preferences opens the shared modal page. The test opens it, pictures it when
    // asked, and closes it before the operations, so a dialog that does not load or does not
    // cover the workbench is caught here rather than by eye.
    {
        QQuickItem* preferences = find_item(content, QStringLiteral("preferencesButton"));
        if (preferences == nullptr)
        {
            std::fprintf(stderr, "the Preferences button is not in the window\n");
            return 1;
        }
        QMetaObject::invokeMethod(preferences, "click");
        QCoreApplication::processEvents();
        QObject* preferences_dialog = window->findChild<QObject*>(QStringLiteral("preferencesDialog"));
        if (preferences_dialog == nullptr || !preferences_dialog->property("opened").toBool())
        {
            std::fprintf(stderr, "the Preferences dialog did not open\n");
            return 1;
        }
        if (argc > 4)
        {
            const QImage preferences_picture = window->grabWindow();
            if (preferences_picture.isNull() || !preferences_picture.save(QString::fromLocal8Bit(argv[4])))
            {
                std::fprintf(stderr, "the Preferences picture could not be saved\n");
                return 1;
            }
        }
        QMetaObject::invokeMethod(preferences_dialog, "close");
        QCoreApplication::processEvents();

        QQuickItem* about = find_item(content, QStringLiteral("aboutButton"));
        if (about == nullptr)
        {
            std::fprintf(stderr, "the About button is not in the window\n");
            return 1;
        }
        QMetaObject::invokeMethod(about, "click");
        QCoreApplication::processEvents();
        QObject* about_dialog = window->findChild<QObject*>(QStringLiteral("aboutDialog"));
        if (about_dialog == nullptr || !about_dialog->property("opened").toBool())
        {
            std::fprintf(stderr, "the About dialog did not open\n");
            return 1;
        }
        if (argc > 5)
        {
            const QImage about_picture = window->grabWindow();
            if (about_picture.isNull() || !about_picture.save(QString::fromLocal8Bit(argv[5])))
            {
                std::fprintf(stderr, "the About picture could not be saved\n");
                return 1;
            }
        }
        QMetaObject::invokeMethod(about_dialog, "close");
        QCoreApplication::processEvents();
    }

    // What the operator does: find the target, take it from the list, read its configuration.
    model.findTargets();
    if (!wait_until_idle(model))
    {
        std::fprintf(stderr, "FIND did not end\n");
        return 1;
    }
    model.selectTarget(0);
    model.readInformation();
    if (!wait_until_idle(model))
    {
        std::fprintf(stderr, "the Information operation did not end\n");
        return 1;
    }
    QCoreApplication::processEvents();

    if (model.resultState() != 1 || model.thwIdPos() != QStringLiteral("HNPFMS_L"))
    {
        std::fprintf(stderr, "the operation did not complete: %s\n", qPrintable(model.resultText()));
        return 1;
    }

    // Then a media set, its load ticked in the window, and the upload started from its button.
    // Choosing the set checks it (validateOnOpen is on), and the Upload button stays disabled
    // until the check has passed.
    // (The simulated target does one operation at a time: it starts again from nothing.)
    rig.target.reset_operation();
    model.chooseMediaSet(fixture.url());
    if (!wait_until_idle(model) || !model.mediaSetError().isEmpty())
    {
        std::fprintf(stderr, "the media set was not read: %s\n", qPrintable(model.mediaSetError()));
        return 1;
    }
    if (model.validationState() != 2)
    {
        std::fprintf(stderr, "the media set check did not pass\n");
        return 1;
    }
    QCoreApplication::processEvents();
    QQuickItem* check = find_item(content, QStringLiteral("loadCheck"));
    QQuickItem* upload = find_item(content, QStringLiteral("uploadButton"));
    if (check == nullptr || upload == nullptr)
    {
        std::fprintf(stderr, "the load's check box or the Upload button is not in the window\n");
        return 1;
    }
    if (upload->isEnabled() || model.canUpload())
    {
        std::fprintf(stderr, "Upload is enabled with no load ticked\n");
        return 1;
    }
    QMetaObject::invokeMethod(check, "click");
    QCoreApplication::processEvents();
    if (model.selectedLoadCount() != 1 || !upload->isEnabled())
    {
        std::fprintf(stderr, "ticking the load in the window did not enable Upload\n");
        return 1;
    }
    QMetaObject::invokeMethod(upload, "click");
    if (!model.busy() || !wait_until_idle(model))
    {
        std::fprintf(stderr, "the upload did not start or did not end\n");
        return 1;
    }
    QCoreApplication::processEvents();
    if (model.resultState() != 1 || model.progress() != 100 ||
        rig.target.log().files_read.size() != fixture.load.item.reads.size())
    {
        std::fprintf(stderr, "the upload did not complete: %s\n", qPrintable(model.resultText()));
        return 1;
    }

    // Then a media defined download through the window's own controls: the file name is typed,
    // added to the list and the Download button pressed. The file lands in a
    // DNLD_DATA_HNPFMS_L_1 directory under the download directory.
    // (The simulated target does one operation at a time: it starts again from nothing.)
    rig.target.reset_operation();
    const std::filesystem::path download_root = fixture.directory.path() / "downloads";
    model.setDownloadDirectory(QString::fromStdU16String(download_root.u16string()));
    QQuickItem* media_mode = find_item(content, QStringLiteral("downloadModeMedia"));
    QQuickItem* name_field = find_item(content, QStringLiteral("downloadFileName"));
    QQuickItem* add = find_item(content, QStringLiteral("addDownloadButton"));
    QQuickItem* download = find_item(content, QStringLiteral("downloadButton"));
    if (media_mode == nullptr || name_field == nullptr || add == nullptr || download == nullptr)
    {
        std::fprintf(stderr, "the download controls are not in the window\n");
        return 1;
    }
    QMetaObject::invokeMethod(media_mode, "click"); // media defined (the default)
    QCoreApplication::processEvents();
    if (download->isEnabled())
    {
        std::fprintf(stderr, "Download is enabled with no file named\n");
        return 1;
    }
    name_field->setProperty("text", QStringLiteral("A.BIN"));
    QCoreApplication::processEvents();
    QMetaObject::invokeMethod(add, "click");
    QCoreApplication::processEvents();
    if (!download->isEnabled() || qobject_cast<adl::app::DownloadModel*>(model.downloads())->rowCount() != 1)
    {
        std::fprintf(stderr, "adding the file name in the window did not enable Download\n");
        return 1;
    }
    QMetaObject::invokeMethod(download, "click");
    if (!model.busy() || !wait_until_idle(model))
    {
        std::fprintf(stderr, "the download did not start or did not end\n");
        return 1;
    }
    QCoreApplication::processEvents();
    const std::u8string generic = download_root.generic_u8string();
    const std::string path =
        std::string{reinterpret_cast<const char*>(generic.data()), generic.size()} + "/DNLD_DATA_HNPFMS_L_1/A.BIN";
    const std::optional<std::vector<std::byte>> got = rig.loader_host.fake_files().get(path);
    if (model.resultState() != 1 || !got.has_value() || *got != plan.files[0].content ||
        rig.target.log().files_written != std::vector<std::string>{"A.BIN"})
    {
        std::fprintf(stderr, "the download did not complete: %s\n", qPrintable(model.resultText()));
        return 1;
    }

    const QImage picture = window->grabWindow();
    if (picture.isNull())
    {
        std::fprintf(stderr, "the window could not be drawn\n");
        return 1;
    }
    if (argc > 1 && !picture.save(QString::fromLocal8Bit(argv[1])))
    {
        std::fprintf(stderr, "the picture could not be saved\n");
        return 1;
    }

    // The two scripts that read right to left are Hebrew and Yiddish; Qt's own locale data has to
    // agree, because the layout direction is taken from it.
    if (QLocale(QStringLiteral("yi")).textDirection() != Qt::RightToLeft ||
        QLocale(QStringLiteral("he")).textDirection() != Qt::RightToLeft)
    {
        std::fprintf(stderr, "Qt does not know Yiddish and Hebrew as right-to-left languages\n");
        return 1;
    }
    for (const QString& code : {QStringLiteral("en"), QStringLiteral("fr"), QStringLiteral("es"), QStringLiteral("de"),
                                QStringLiteral("zh"), QStringLiteral("ja")})
    {
        if (QLocale(code).textDirection() != Qt::LeftToRight)
        {
            std::fprintf(stderr, "Qt takes %s for a right-to-left language\n", qPrintable(code));
            return 1;
        }
    }

    // Every catalogue the window ships: the running window is retranslated, the layout direction
    // follows the script (right to left for Hebrew and Yiddish), and the text really changes -
    // which also proves the .qm resource was found and installed.
    for (const QString& code : {QStringLiteral("fr"), QStringLiteral("es"), QStringLiteral("de"), QStringLiteral("yi"),
                                QStringLiteral("he"), QStringLiteral("zh"), QStringLiteral("ja")})
    {
        if (!language_manager.setLanguage(code))
        {
            std::fprintf(stderr, "the language %s is not accepted\n", qPrintable(code));
            return 1;
        }
        QCoreApplication::processEvents();
        if (language_manager.currentLanguage() != code)
        {
            std::fprintf(stderr, "the language did not change to %s\n", qPrintable(code));
            return 1;
        }
        const bool right_to_left = QLocale(code).textDirection() == Qt::RightToLeft;
        if ((QGuiApplication::layoutDirection() == Qt::RightToLeft) != right_to_left)
        {
            std::fprintf(stderr, "the layout direction did not follow %s\n", qPrintable(code));
            return 1;
        }
        if (QCoreApplication::translate("PreferencesDialog", "Preferences") == QStringLiteral("Preferences"))
        {
            std::fprintf(stderr, "the %s catalogue was not loaded\n", qPrintable(code));
            return 1;
        }
        // The typeface follows the script: Archivo has no Hebrew letters, so Hebrew and Yiddish are
        // set in the bundled Noto Sans Hebrew (the picture below shows it).
        const QString family = QGuiApplication::font().family();
        const bool hebrew_script = right_to_left;
        if ((family == QStringLiteral("Noto Sans Hebrew")) != hebrew_script)
        {
            std::fprintf(stderr, "the window's typeface for %s is %s\n", qPrintable(code), qPrintable(family));
            return 1;
        }
        // What the engine says is translated too, not only the QML: an outcome of the 615A
        // library reads in the language of the moment.
        if (code != QStringLiteral("en") && adl::app::Wording::outcome(adl::arinc615a::OutcomeCode::denied).text() ==
                                                QStringLiteral("denied by the target"))
        {
            std::fprintf(stderr, "the engine's wording is not translated in %s\n", qPrintable(code));
            return 1;
        }
        // With a third argument, what the window looks like right to left is kept too: Hebrew
        // mirrors the whole layout, not only its text.
        if (code == QStringLiteral("he") && argc > 3)
        {
            const QImage right_to_left_picture = window->grabWindow();
            if (right_to_left_picture.isNull() || !right_to_left_picture.save(QString::fromLocal8Bit(argv[3])))
            {
                std::fprintf(stderr, "the right-to-left picture could not be saved\n");
                return 1;
            }
        }
    }
    language_manager.setLanguage(QStringLiteral("en"));
    QCoreApplication::processEvents();
    if (QGuiApplication::layoutDirection() == Qt::RightToLeft)
    {
        std::fprintf(stderr, "English did not put the layout back to left to right\n");
        return 1;
    }

    if (complaints != 0)
    {
        std::fprintf(stderr, "%d QML complaint(s)\n", complaints);
        return 1;
    }
    // The engine first, while the language manager is still alive for the teardown's bindings;
    // the profiler that listened to the window after it.
    engine.reset();
    frame_profiler.reset();
    return 0;
}
