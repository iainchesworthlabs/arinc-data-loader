#include "ui_harness.hpp"

#include <chrono>
#include <cstddef>
#include <expected>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#include <QQuickItem>
#include <QSettings>
#include <QStringList>

#include "adl/app/models.hpp"
#include "adl/app/settings.hpp"
#include "bytes.hpp"

namespace adl::ui_test {
namespace {

// A target that answers FIND with the identity the suite expects, holds two files for a media
// defined download, and reads the sample load for an upload.
adl::test::SimPlan plan_of(const adl::test::UploadFixture& fixture) {
    adl::test::SimPlan plan = fixture.plan();
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    plan.files = {
        adl::test::DownloadItem{"a.bin", adl::test::bytes_of("downloaded"), "a file"},
        adl::test::DownloadItem{"b.bin", adl::test::bytes_of("other"), ""},
    };
    return plan;
}

std::unique_ptr<adl::app::AppSettings> settings_of(const adl::test::TempDir& directory) {
    const auto file = directory.path() / "ui-settings.ini";
    return std::make_unique<adl::app::AppSettings>(std::make_unique<QSettings>(
        QString::fromStdU16String(file.u16string()), QSettings::IniFormat));
}

QString text_of(const std::vector<std::byte>& bytes) {
    return QString::fromUtf8(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<qsizetype>(bytes.size()));
}

}  // namespace

Harness::Harness(QObject* parent)
    : QObject(parent),
      settings_directory_(),
      fixture_(),
      rig_(std::make_unique<adl::test::Rig>(plan_of(fixture_))),
      gated_(std::make_unique<adl::test::GatedHost>(rig_->loader_host)),
      model_(std::make_unique<adl::app::LoaderModel>(
          [this]() -> std::expected<adl::app::HostLease, QString> {
              {
                  std::unique_lock lock(hold_mutex_);
                  if (hold_) {
                      held_ = true;
                      hold_changed_.notify_all();
                      hold_changed_.wait(lock, [this] { return !hold_; });
                      held_ = false;
                  }
              }
              adl::app::HostLease lease;
              lease.host = gated_.get();
              return lease;
          },
          settings_of(settings_directory_))) {
    ready_ = fixture_.write(*rig_);
}

Harness::~Harness() {
    // A job held in the middle of a transfer would keep the model's destructor waiting for ever.
    releaseJob();
}

QString Harness::mediaSetPath() const {
    return QString::fromStdU16String(fixture_.directory.path().u16string());
}

QUrl Harness::mediaSetUrl() const {
    return fixture_.url();
}

QString Harness::initialDownloadDirectory() const {
    return QString::fromStdU16String((fixture_.directory.path() / "initial-downloads").u16string());
}

QVariantMap Harness::storedPreferences() const {
    // Another model over the same settings file, as the next launch would make it.
    adl::app::LoaderModel next(
        []() -> std::expected<adl::app::HostLease, QString> {
            return std::unexpected(QStringLiteral("no host in this model"));
        },
        settings_of(settings_directory_));
    QVariantMap map;
    map.insert(QStringLiteral("defaultTargetAddress"), next.defaultTargetAddress());
    map.insert(QStringLiteral("defaultThwIdPos"), next.defaultThwIdPos());
    map.insert(QStringLiteral("downloadDirectory"), next.downloadDirectory());
    map.insert(QStringLiteral("controlPort"), next.controlPort());
    map.insert(QStringLiteral("logCapacity"), next.logCapacity());
    map.insert(QStringLiteral("autoFindBeforeOperation"), next.autoFindBeforeOperation());
    map.insert(QStringLiteral("validateOnOpen"), next.validateOnOpen());
    map.insert(QStringLiteral("firstRun"), next.firstRun());
    return map;
}

QString Harness::downloadDirectory() const {
    return QString::fromStdU16String((fixture_.directory.path() / "downloads").u16string());
}

QStringList Harness::expectedUploadFiles() const {
    QStringList names;
    for (const adl::test::ReadItem& item : fixture_.load.item.reads) {
        names.append(QString::fromStdString(item.name));
    }
    return names;
}

QStringList Harness::targetFilesRead() const {
    QStringList names;
    for (const std::string& name : rig_->target.log().files_read) {
        names.append(QString::fromStdString(name));
    }
    return names;
}

int Harness::targetFilesReadCount() const {
    return static_cast<int>(rig_->target.log().files_read.size());
}

std::optional<std::vector<std::byte>> Harness::downloadedBytes(const QString& name,
                                                               int number) const {
    const std::u8string directory = (fixture_.directory.path() / "downloads").generic_u8string();
    const std::string path =
        std::string{reinterpret_cast<const char*>(directory.data()), directory.size()} +
        "/DNLD_DATA_HNPFMS_L_" + std::to_string(number) + "/" + name.toStdString();
    return rig_->loader_host.fake_files().get(path);
}

bool Harness::downloadedFileExists(const QString& name, int number) const {
    return downloadedBytes(name, number).has_value();
}

QString Harness::downloadedFileText(const QString& name, int number) const {
    const auto bytes = downloadedBytes(name, number);
    return bytes.has_value() ? text_of(*bytes) : QString{};
}

namespace {

void collect(QObject* root, const QString& name, QVariantList& found, std::set<QObject*>& visited) {
    if (root == nullptr || !visited.insert(root).second) {
        return;
    }
    if (root->objectName() == name) {
        found.append(QVariant::fromValue(root));
    }
    // Visual children and QObject children both, as findItem does; an item is listed once.
    if (auto* item = qobject_cast<QQuickItem*>(root)) {
        for (QQuickItem* child : item->childItems()) {
            collect(child, name, found, visited);
        }
    }
    for (QObject* child : root->children()) {
        collect(child, name, found, visited);
    }
}

}  // namespace

QVariantList Harness::findItems(QObject* root, const QString& name) const {
    QVariantList found;
    std::set<QObject*> visited;
    collect(root, name, found, visited);
    return found;
}

int Harness::targetAbortsSeen() const {
    return static_cast<int>(rig_->target.log().aborts_seen.size());
}

int Harness::targetFilesWrittenCount() const {
    return static_cast<int>(rig_->target.log().files_written.size());
}

QStringList Harness::targetFilesWritten() const {
    QStringList names;
    for (const std::string& name : rig_->target.log().files_written) {
        names.append(QString::fromStdString(name));
    }
    return names;
}

QObject* Harness::findItemByText(QObject* root, const QString& text) const {
    if (root == nullptr) {
        return nullptr;
    }
    if (root->property("text").toString() == text && root->property("visible").toBool()) {
        return root;
    }
    if (auto* item = qobject_cast<QQuickItem*>(root)) {
        for (QQuickItem* child : item->childItems()) {
            if (QObject* found = findItemByText(child, text)) {
                return found;
            }
        }
    }
    for (QObject* child : root->children()) {
        if (QObject* found = findItemByText(child, text)) {
            return found;
        }
    }
    return nullptr;
}

QObject* Harness::findItem(QObject* root, const QString& name) const {
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    if (auto* item = qobject_cast<QQuickItem*>(root)) {
        for (QQuickItem* child : item->childItems()) {
            if (QObject* found = findItem(child, name)) {
                return found;
            }
        }
    }
    for (QObject* child : root->children()) {
        if (QObject* found = findItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

int Harness::targetCount() const {
    const auto* targets = qobject_cast<adl::app::TargetModel*>(model_->targets());
    return targets == nullptr ? 0 : targets->rowCount();
}

int Harness::configurationCount() const {
    const auto* rows = qobject_cast<adl::app::ConfigurationModel*>(model_->configuration());
    return rows == nullptr ? 0 : rows->rowCount();
}

int Harness::loadCount() const {
    const auto* loads = qobject_cast<adl::app::LoadModel*>(model_->loads());
    return loads == nullptr ? 0 : loads->rowCount();
}

int Harness::fileCount() const {
    const auto* files = qobject_cast<adl::app::FileModel*>(model_->files());
    return files == nullptr ? 0 : files->rowCount();
}

void Harness::showTargets(int count) {
    auto* targets = qobject_cast<adl::app::TargetModel*>(model_->targets());
    if (targets == nullptr) {
        return;
    }
    QList<adl::app::FoundTarget> found;
    for (int i = 0; i < count; ++i) {
        adl::app::FoundTarget target;
        target.address = QStringLiteral("10.0.0.%1").arg(2 + i);
        target.hardware_id = QStringLiteral("HW%1").arg(i + 1);
        target.type_name = QStringLiteral("FMS");
        target.position = i % 2 == 0 ? QStringLiteral("L") : QStringLiteral("R");
        target.literal_name = QStringLiteral("TARGET %1").arg(i + 1);
        target.manufacturer_code = QStringLiteral("HNP");
        found.append(target);
    }
    targets->reset(found);
}

void Harness::addEvent(int severity, const QString& text) {
    auto* events = qobject_cast<adl::app::EventModel*>(model_->events());
    if (events == nullptr) {
        return;
    }
    events->append(adl::app::EventEntry{static_cast<adl::app::Severity>(severity),
                                        adl::app::Message::verbatim(text)});
}

int Harness::eventCount() const {
    const auto* events = qobject_cast<adl::app::EventModel*>(model_->events());
    return events == nullptr ? 0 : events->rowCount();
}

QString Harness::eventText() const {
    const auto* events = qobject_cast<adl::app::EventModel*>(model_->events());
    if (events == nullptr) {
        return {};
    }
    QStringList lines;
    for (int row = 0; row < events->rowCount(); ++row) {
        lines.append(events->data(events->index(row), adl::app::EventModel::TextRole).toString());
    }
    return lines.join(QLatin1Char('\n'));
}

int Harness::downloadCount() const {
    const auto* downloads = qobject_cast<adl::app::DownloadModel*>(model_->downloads());
    return downloads == nullptr ? 0 : downloads->rowCount();
}

void Harness::resetTarget() {
    rig_->target.reset_operation();
    adl::test::SimPlan& plan = rig_->target.plan();
    plan.init_status = adl::arinc615a::StatusCode::accepted;
    plan.init_text.clear();
    plan.init_wait_seconds = 0;
    plan.init_wait_always = false;
    plan.init_silent = false;
    plan.think_time = std::chrono::seconds(1);
    plan.status_period = std::chrono::seconds(4);
    plan.work_time = {};
    plan.abort_after_statuses.reset();
    plan.silent_after_statuses.reset();
}

void Harness::resetDownloads() {
    // Back to an empty media defined list. (remove_row refuses while the list is the target's own
    // choice, so removing rows one by one would never end after an operator defined download.)
    auto* downloads = qobject_cast<adl::app::DownloadModel*>(model_->downloads());
    if (downloads != nullptr) {
        downloads->reset_media({});
    }
}

void Harness::makeTargetRefuse() {
    adl::test::SimPlan& plan = rig_->target.plan();
    plan.init_status = adl::arinc615a::StatusCode::operation_denied;
    plan.init_text = "the target refuses the operation";
}

void Harness::holdNextJob() {
    const std::scoped_lock lock(hold_mutex_);
    hold_ = true;
}

bool Harness::jobHeld() const {
    const std::scoped_lock lock(hold_mutex_);
    return held_;
}

void Harness::makeTargetSlow() {
    rig_->target.plan().work_time = std::chrono::seconds(60);
}

void Harness::holdUploadAtSecondFile() {
    // The application opens every file of the set once before an upload starts, to see that none
    // changed since the check; the target's own requests come after those.
    gated_->arm(adl::test::GatedHost::Event::open_read,
                static_cast<unsigned>(model_->fileCount()) + 2);
}

void Harness::holdDownloadAtSecondFile() {
    gated_->arm(adl::test::GatedHost::Event::create, 2);
}

bool Harness::gateReached() const {
    return gated_->reached();
}

void Harness::releaseJob() {
    gated_->release();
    {
        const std::scoped_lock lock(hold_mutex_);
        hold_ = false;
    }
    hold_changed_.notify_all();
}

}  // namespace adl::ui_test
