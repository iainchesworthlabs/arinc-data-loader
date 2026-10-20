#pragma once

// What the Qt Quick Test suite drives: the application's real LoaderModel on its engine thread,
// against the simulated target on the fake platform, with a media set written to disk and mirrored
// into the fake host's file system. The harness is the test's own seam: it builds the rig, resets
// it between test cases and answers the questions a QML test cannot ask the model directly (what
// the target read, what a download wrote, how many rows a list holds).

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QSemaphore>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include "adl/app/loader_model.hpp"
#include "rig.hpp"
#include "upload_fixture.hpp"

namespace adl::ui_test {

class Harness : public QObject {
    Q_OBJECT

public:
    explicit Harness(QObject* parent = nullptr);
    ~Harness() override;

    [[nodiscard]] adl::app::LoaderModel& model() noexcept { return *model_; }

    // False when the media set could not be written; the QML suite fails rather than runs.
    [[nodiscard]] Q_INVOKABLE bool ready() const noexcept { return ready_; }
    [[nodiscard]] Q_INVOKABLE QString mediaSetPath() const;
    [[nodiscard]] Q_INVOKABLE QUrl mediaSetUrl() const;
    [[nodiscard]] Q_INVOKABLE QString downloadDirectory() const;

    // What a whole upload should have the target read, and what it did read.
    [[nodiscard]] Q_INVOKABLE QStringList expectedUploadFiles() const;
    [[nodiscard]] Q_INVOKABLE QStringList targetFilesRead() const;
    [[nodiscard]] Q_INVOKABLE int targetFilesReadCount() const;

    // A download's file, as the fake host's file system holds it: the directory is
    // <download directory>/DNLD_DATA_HNPFMS_L_<number>/<name>.
    [[nodiscard]] Q_INVOKABLE bool downloadedFileExists(const QString& name, int number = 1) const;
    [[nodiscard]] Q_INVOKABLE QString downloadedFileText(const QString& name, int number = 1) const;

    // An item anywhere below `root`, visual children and QObject children both: QML's findChild
    // only walks the QObject tree, and a Repeater's delegates are visual children.
    [[nodiscard]] Q_INVOKABLE QObject* findItem(QObject* root, const QString& name) const;

    // Every item called `name` below `root`, in tree order (a Repeater's delegates, for one).
    [[nodiscard]] Q_INVOKABLE QVariantList findItems(QObject* root, const QString& name) const;

    // The rows of the lists the window shows, for assertions that a binding or a button filled one.
    [[nodiscard]] Q_INVOKABLE int targetCount() const;
    [[nodiscard]] Q_INVOKABLE int configurationCount() const;
    [[nodiscard]] Q_INVOKABLE int loadCount() const;
    [[nodiscard]] Q_INVOKABLE int fileCount() const;
    [[nodiscard]] Q_INVOKABLE int eventCount() const;
    [[nodiscard]] Q_INVOKABLE int downloadCount() const;
    [[nodiscard]] Q_INVOKABLE QString eventText() const;

    // Puts `count` found targets in the FIND list (10.0.0.2, 10.0.0.3, ...) without a FIND, and
    // adds one line of the given severity (0 info, 1 notice, 2 warning, 3 error) to the log.
    Q_INVOKABLE void showTargets(int count);
    Q_INVOKABLE void addEvent(int severity, const QString& text);

    // Between test cases: the target is ready for another operation and behaves.
    Q_INVOKABLE void resetTarget();
    Q_INVOKABLE void resetDownloads();
    // The target refuses the next operation.
    Q_INVOKABLE void makeTargetRefuse();
    // Holds the engine's thread just before it takes the host, so a UI test can be certain a job
    // is running (and press Abort) however fast the fake platform would finish it.
    Q_INVOKABLE void holdNextJob();
    Q_INVOKABLE void releaseJob();

private:
    void collectItems(QObject* root, const QString& name, QVariantList& found) const;
    [[nodiscard]] std::optional<std::vector<std::byte>> downloadedBytes(const QString& name,
                                                                        int number) const;

    // The settings live outside the media set's directory: a stray file at the root of a media
    // set is exactly what the tree reader refuses.
    adl::test::TempDir settings_directory_;
    adl::test::UploadFixture fixture_;
    std::unique_ptr<adl::test::Rig> rig_;
    std::unique_ptr<adl::app::LoaderModel> model_;
    std::atomic<bool> hold_{false};
    QSemaphore released_;
    bool ready_ = false;
};

}  // namespace adl::ui_test
