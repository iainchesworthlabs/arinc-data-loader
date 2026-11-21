// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/ui/licence_model.hpp"

#include "adl/licence/product.hpp"
#include "adl/licence/words.hpp"
#include "adl/licence_platform/files.hpp"
#include "adl/licence_platform/locations.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/paths.hpp"
#include "adl/ui/build_info.hpp"

#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QSettings>
#include <QTimeZone>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <utility>

namespace adl::ui
{

namespace lic = adl::licence;
namespace store = adl::licence::store;

namespace
{

constexpr int refresh_interval_ms = 60 * 60 * 1000;

// Where the notices a person put away are remembered, in the program's own settings.
constexpr auto settings_group = "licence";
constexpr auto dismissed_key = "dismissed";
constexpr auto remind_key = "remindAfter";

constexpr auto issues_address = "https://github.com/iainchesworthlabs/arinc-data-loader/issues";
constexpr auto project_address = "https://github.com/iainchesworthlabs/arinc-data-loader";

[[nodiscard]] QString text_of(std::string_view bytes)
{
    return QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size()));
}

[[nodiscard]] QDate local_date(lic::TimePoint time)
{
    return QDateTime::fromSecsSinceEpoch(time.time_since_epoch().count(), QTimeZone::UTC).toLocalTime().date();
}

// "13 November 2027", in the language and the time zone of this computer.
[[nodiscard]] QString date_text(lic::TimePoint time)
{
    return QLocale().toString(local_date(time), QLocale::LongFormat);
}

// The key a dismissed notice is remembered by: the licence and the state it was in.
[[nodiscard]] QString notice_key(const lic::Status& status)
{
    const QString number = status.value ? text_of(status.value->licence_id) : QString{};
    return number + QLatin1Char('|') + text_of(lic::state_name(status.state));
}

} // namespace

struct LicenceModel::Impl
{
    LicenceEnvironment env;
    // What the model made for itself when it was made for this computer.
    std::unique_ptr<platform::Host> host;
    std::unique_ptr<licence_platform::PlatformFiles> platform_files;

    store::Overview overview;
    lic::Status best;
    lic::ClockState clock;
    lic::TimePoint now;
    QVariantMap import_result;
    // A clock notice is put away for this run only.
    QString clock_dismissed_for;
};

LicenceModel::LicenceModel(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>())
{
    std::string host_name;
    if (auto host = platform::make_host())
    {
        impl_->host = std::move(*host);
        impl_->platform_files = std::make_unique<licence_platform::PlatformFiles>(impl_->host->files());
        host_name = impl_->host->info().name;
    }
    // ADL_SETTINGS_DIR moves the preferences, and the licence folders and the clock file with them, so that a
    // test run never reads a real licence. The variable is UTF-8 text, and no name goes through a code page.
    const QByteArray settings = qEnvironmentVariable("ADL_SETTINGS_DIR").toUtf8();
    const std::filesystem::path settings_path{std::u8string{reinterpret_cast<const char8_t*>(settings.constData()),
                                                            static_cast<std::size_t>(settings.size())}};
    impl_->env.folders = licence_platform::folders_for(licence_platform::current_roots(host_name, settings_path));
    impl_->env.files = impl_->platform_files.get();
    impl_->env.now = [] { return lic::TimePoint{std::chrono::seconds{QDateTime::currentSecsSinceEpoch()}}; };
    impl_->env.computer_name = platform::computer_name();
    impl_->env.keys = lic::signed_file::product_trust_list();
    // A time that has certainly passed: when the source of this build was committed, if it says.
    const QDateTime built = QDateTime::fromString(QString::fromLatin1(kGitCommitTime), Qt::ISODate);
    if (built.isValid())
    {
        impl_->env.build_time = lic::TimePoint{std::chrono::seconds{built.toSecsSinceEpoch()}};
    }

    connect(&hourly_, &QTimer::timeout, this, &LicenceModel::refresh);
    hourly_.start(refresh_interval_ms);
    judge();
}

LicenceModel::LicenceModel(LicenceEnvironment environment, QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>())
{
    impl_->env = std::move(environment);
    connect(&hourly_, &QTimer::timeout, this, &LicenceModel::refresh);
    hourly_.start(refresh_interval_ms);
    judge();
}

LicenceModel::~LicenceModel() = default;

// ---- judging ---------------------------------------------------------------------------------

void LicenceModel::judge()
{
    const LicenceEnvironment& env = impl_->env;
    impl_->now = env.now ? env.now() : lic::TimePoint{};
    impl_->overview = {};
    impl_->best = lic::no_licence();
    if (env.files == nullptr)
    {
        return;
    }
    impl_->clock = store::load_clock(*env.files, env.folders);
    lic::Context where;
    where.now = impl_->now;
    where.latest_seen = impl_->clock.latest_seen;
    where.build_time = env.build_time;
    where.computer_name = env.computer_name;
    where.keys = env.keys;
    impl_->overview = store::scan(*env.files, env.folders, where);
    impl_->best = impl_->overview.best;

    // The latest time seen only moves forward. A computer with no licence file keeps no clock file.
    const lic::ClockState updated = lic::observe(impl_->clock, impl_->now);
    if (!impl_->overview.licences.empty() && !(updated == impl_->clock))
    {
        static_cast<void>(store::save_clock(*env.files, env.folders, updated));
    }
}

void LicenceModel::refresh()
{
    judge();
    Q_EMIT changed();
}

void LicenceModel::retranslate()
{
    Q_EMIT changed();
    Q_EMIT importResultChanged();
}

// ---- words -----------------------------------------------------------------------------------

namespace
{

[[nodiscard]] QString edition_words(lic::Edition edition, bool trial)
{
    if (trial)
    {
        return edition == lic::Edition::enterprise ? LicenceModel::tr("Enterprise trial")
                                                   : LicenceModel::tr("Pro trial");
    }
    return edition == lic::Edition::enterprise ? LicenceModel::tr("Enterprise") : LicenceModel::tr("Pro");
}

[[nodiscard]] QString state_label(lic::State state)
{
    switch (state)
    {
    case lic::State::none:
        return LicenceModel::tr("Community");
    case lic::State::valid:
        return LicenceModel::tr("Valid");
    case lic::State::expiring:
        return LicenceModel::tr("Ends soon");
    case lic::State::grace:
    case lic::State::expired:
        return LicenceModel::tr("Ended");
    case lic::State::not_yet_valid:
        return LicenceModel::tr("Not started");
    case lic::State::clock_suspect:
        return LicenceModel::tr("Check the date");
    case lic::State::invalid:
        return LicenceModel::tr("Not valid");
    }
    return {};
}

[[nodiscard]] QString state_tone(lic::State state)
{
    switch (state)
    {
    case lic::State::valid:
        return QStringLiteral("ok");
    case lic::State::expiring:
    case lic::State::grace:
    case lic::State::expired:
    case lic::State::clock_suspect:
        return QStringLiteral("warn");
    case lic::State::invalid:
        return QStringLiteral("fail");
    case lic::State::none:
    case lic::State::not_yet_valid:
        break;
    }
    return QStringLiteral("neutral");
}

} // namespace

QString LicenceModel::edition() const
{
    return text_of(lic::edition_name(impl_->best));
}

QString LicenceModel::editionText() const
{
    if (!impl_->best.active() || !impl_->best.value)
    {
        return tr("Community");
    }
    return edition_words(impl_->best.value->edition, impl_->best.value->kind == lic::LicenceKind::trial);
}

QString LicenceModel::stamp() const
{
    // The edition in force and, when a licence is in force, its number. The number of a licence that has ended is not
    // given: the stamp says what the program runs as.
    if (impl_->best.active() && impl_->best.value)
    {
        return tr("Edition: %1, licence %2").arg(editionText(), text_of(impl_->best.value->licence_id));
    }
    return tr("Edition: %1").arg(editionText());
}

QString LicenceModel::state() const
{
    return text_of(lic::state_name(impl_->best.state));
}

QString LicenceModel::stateLabel() const
{
    return state_label(impl_->best.state);
}

QString LicenceModel::stateTone() const
{
    return state_tone(impl_->best.state);
}

namespace
{

// The sentence for a reason that makes a licence file not valid, for a licence that names `value` (which may be null).
[[nodiscard]] QString invalid_sentence(lic::Reason reason, bool no_keys, const QString& computer_name)
{
    if (no_keys)
    {
        // A list with no key in it checks nothing, so an unknown key is not "a newer version". A release has
        // ADL's keys, so only a build made without them, or a test, comes here.
        return LicenceModel::tr("This version of ARINC Data Loader cannot check licence files yet, so it accepts none. "
                                "The Community edition works as it always did.");
    }
    switch (reason)
    {
    case lic::Reason::newer_version:
        return LicenceModel::tr("This licence needs a newer version of ARINC Data Loader. Install the latest version, "
                                "then import the licence again.");
    case lic::Reason::revoked:
        return LicenceModel::tr("This licence was withdrawn. Contact support.");
    case lic::Reason::wrong_computer:
        return LicenceModel::tr(
                   "This licence is for other computers. This computer is %1. Ask your administrator for a "
                   "licence that names it.")
            .arg(computer_name);
    case lic::Reason::wrong_product:
        return LicenceModel::tr("This licence is for another product. Ask for an ARINC Data Loader licence.");
    case lic::Reason::malformed:
    case lic::Reason::bad_signature:
    default:
        break;
    }
    return LicenceModel::tr(
        "This file is not a valid ARINC Data Loader licence. It may be damaged or incomplete. Ask for "
        "the file again.");
}

} // namespace

QString LicenceModel::sentence() const
{
    const lic::Status& best = impl_->best;
    const QString computer = text_of(impl_->env.computer_name);
    switch (best.state)
    {
    case lic::State::none:
        return tr("No licence is installed. This is the Community edition. Every function of the programs is available "
                  "without a licence.");
    case lic::State::valid:
        return tr("Valid until %1.").arg(date_text(best.value->expires_at));
    case lic::State::expiring:
        if (best.days_left <= 0)
        {
            return tr("Valid until %1. It ends today.").arg(date_text(best.value->expires_at));
        }
        return tr("Valid until %1. %n day(s) left.", nullptr, best.days_left).arg(date_text(best.value->expires_at));
    case lic::State::grace:
        return tr("Ended on %1. The grace period lasts until %2.")
            .arg(date_text(best.value->expires_at), date_text(best.grace_ends));
    case lic::State::expired:
        return tr("Ended on %1. This is the Community edition again. Nothing you made is lost: media sets, receipts "
                  "and "
                  "logs are unchanged.")
            .arg(date_text(best.value->expires_at));
    case lic::State::not_yet_valid:
        return tr("Starts on %1.").arg(date_text(best.value->starts_at));
    case lic::State::clock_suspect:
        return tr("The date on this computer looks wrong, so the licence cannot be judged until the date is right.");
    case lic::State::invalid:
        return invalid_sentence(best.reason, impl_->env.keys.empty(), computer);
    }
    return {};
}

QString LicenceModel::chipText() const
{
    const lic::Status& best = impl_->best;
    const QString words = editionText();
    switch (best.state)
    {
    case lic::State::valid:
        return words;
    case lic::State::expiring:
        if (best.days_left <= 0)
        {
            return tr("%1, ends today").arg(words);
        }
        return tr("%1, %n day(s) left", nullptr, best.days_left).arg(words);
    case lic::State::grace:
        return tr("%1, licence ended").arg(words);
    default:
        break;
    }
    return tr("Community");
}

QString LicenceModel::chipTone() const
{
    switch (impl_->best.state)
    {
    case lic::State::none:
        return QStringLiteral("neutral");
    case lic::State::valid:
        return QStringLiteral("ok");
    default:
        return QStringLiteral("warn");
    }
}

QString LicenceModel::chipName() const
{
    return tr("Edition: %1. Open the licence window.").arg(chipText());
}

// ---- the summary -----------------------------------------------------------------------------

bool LicenceModel::hasLicence() const
{
    return impl_->best.value.has_value();
}

QString LicenceModel::licensee() const
{
    return impl_->best.value ? text_of(impl_->best.value->licensee_name) : QString{};
}

QString LicenceModel::licenceNumber() const
{
    return impl_->best.value ? text_of(impl_->best.value->licence_id) : QString{};
}

QString LicenceModel::term() const
{
    if (!impl_->best.value)
    {
        return {};
    }
    return tr("%1 to %2").arg(date_text(impl_->best.value->starts_at), date_text(impl_->best.value->expires_at));
}

QString LicenceModel::seats() const
{
    if (!impl_->best.value)
    {
        return {};
    }
    const int seats = impl_->best.value->seats;
    return seats == 0 ? tr("Site licence") : tr("%n seat(s)", nullptr, seats);
}

QString LicenceModel::computers() const
{
    if (!impl_->best.value || impl_->best.value->machines.empty())
    {
        return {};
    }
    QStringList names;
    for (const std::string& name : impl_->best.value->machines)
    {
        names.push_back(text_of(name));
    }
    return tr("%1. This computer is %2.").arg(names.join(QStringLiteral(", ")), text_of(impl_->env.computer_name));
}

namespace
{

struct FeatureName
{
    std::string_view feature;
    // The words for the feature, and the sentence when there is no licence: which edition has it.
    QString name;
    QString needs;
};

[[nodiscard]] std::vector<FeatureName> known_features()
{
    return {
        {lic::feature_support, LicenceModel::tr("Support"), LicenceModel::tr("Needs a Pro or Enterprise licence.")},
        {lic::feature_data_packs, LicenceModel::tr("Data packs"), LicenceModel::tr("Needs an Enterprise licence.")}};
}

} // namespace

QVariantList LicenceModel::features() const
{
    const lic::Status& best = impl_->best;
    QVariantList rows;
    for (const FeatureName& feature : known_features())
    {
        const lic::Entitlement answer = lic::check(best, feature.feature);
        QString why;
        if (!answer.granted)
        {
            switch (answer.reason)
            {
            case lic::Reason::no_licence:
                why = feature.needs;
                break;
            case lic::Reason::not_in_licence:
                why = tr("Your licence does not include this.");
                break;
            case lic::Reason::expired:
                why = tr("Your licence ended on %1.").arg(best.value ? date_text(best.value->expires_at) : QString{});
                break;
            case lic::Reason::not_yet_valid:
                why = tr("Your licence starts on %1.").arg(best.value ? date_text(best.value->starts_at) : QString{});
                break;
            case lic::Reason::clock_suspect:
                why = tr("The date on this computer looks wrong.");
                break;
            case lic::Reason::wrong_computer:
                why = tr("Your licence is for other computers.");
                break;
            default:
                why = tr("The installed licence is not valid.");
                break;
            }
        }
        QVariantMap row;
        row.insert(QStringLiteral("name"), feature.name);
        row.insert(QStringLiteral("available"), answer.granted);
        row.insert(QStringLiteral("why"), why);
        rows.push_back(row);
    }
    return rows;
}

QVariantList LicenceModel::files() const
{
    QVariantList rows;
    const lic::Status& best = impl_->best;
    bool marked = false;
    for (const store::Entry& entry : impl_->overview.licences)
    {
        const lic::Status& status = entry.status;
        // The file in use is the first one that is the best.
        const bool in_use = !marked && status.state == best.state && status.value == best.value;
        marked = marked || in_use;
        QVariantMap row;
        row.insert(QStringLiteral("name"), text_of(entry.name));
        row.insert(QStringLiteral("licenceNumber"), status.value ? text_of(status.value->licence_id) : QString{});
        row.insert(QStringLiteral("edition"),
                   status.value ? edition_words(status.value->edition, status.value->kind == lic::LicenceKind::trial)
                                : QString{});
        row.insert(QStringLiteral("term"),
                   status.value
                       ? tr("%1 to %2").arg(date_text(status.value->starts_at), date_text(status.value->expires_at))
                       : QString{});
        row.insert(QStringLiteral("stateLabel"), state_label(status.state));
        row.insert(QStringLiteral("tone"), state_tone(status.state));
        row.insert(QStringLiteral("inUse"), in_use);
        row.insert(QStringLiteral("byAdministrator"), entry.origin == store::Origin::machine);
        rows.push_back(row);
    }
    return rows;
}

bool LicenceModel::canRemove() const
{
    return !fileInUse().isEmpty();
}

QString LicenceModel::fileInUse() const
{
    const lic::Status& best = impl_->best;
    for (const store::Entry& entry : impl_->overview.licences)
    {
        if (entry.status.state == best.state && entry.status.value == best.value)
        {
            return entry.origin == store::Origin::user ? text_of(entry.name) : QString{};
        }
    }
    return {};
}

QString LicenceModel::userFolder() const
{
    return QDir::toNativeSeparators(text_of(impl_->env.folders.user));
}

QString LicenceModel::machineFolder() const
{
    return QDir::toNativeSeparators(text_of(impl_->env.folders.machine));
}

QString LicenceModel::computerName() const
{
    return text_of(impl_->env.computer_name);
}

// ---- the notice ------------------------------------------------------------------------------

QString LicenceModel::noticeKind() const
{
    const lic::Status& best = impl_->best;
    QSettings settings;
    settings.beginGroup(QLatin1String(settings_group));
    const QString dismissed = settings.value(QLatin1String(dismissed_key)).toString();
    switch (best.state)
    {
    case lic::State::expiring:
    case lic::State::expired:
        return dismissed == notice_key(best) ? QString{} : QStringLiteral("info");
    case lic::State::grace:
        return settings.value(QLatin1String(remind_key)).toDate() == local_date(impl_->now) ? QString{}
                                                                                            : QStringLiteral("warn");
    case lic::State::clock_suspect:
        return impl_->clock_dismissed_for == notice_key(best) ? QString{} : QStringLiteral("warn");
    default:
        break;
    }
    return {};
}

QString LicenceModel::noticeText() const
{
    const lic::Status& best = impl_->best;
    if (noticeKind().isEmpty())
    {
        return {};
    }
    switch (best.state)
    {
    case lic::State::expiring:
        return tr("Your licence ends on %1.").arg(date_text(best.value->expires_at));
    case lic::State::grace:
        return tr("Your licence ended on %1. Its grace period ends on %2.")
            .arg(date_text(best.value->expires_at), date_text(best.grace_ends));
    case lic::State::expired:
        return tr("Your licence has ended. This is the Community edition again, and nothing you made is lost.");
    case lic::State::clock_suspect:
        return tr("The date on this computer looks wrong, so the licence cannot be judged.");
    default:
        break;
    }
    return {};
}

QString LicenceModel::noticeDismissText() const
{
    return impl_->best.state == lic::State::grace ? tr("Remind me tomorrow") : tr("Dismiss");
}

void LicenceModel::dismissNotice()
{
    const lic::Status& best = impl_->best;
    QSettings settings;
    settings.beginGroup(QLatin1String(settings_group));
    switch (best.state)
    {
    case lic::State::expiring:
    case lic::State::expired:
        settings.setValue(QLatin1String(dismissed_key), notice_key(best));
        break;
    case lic::State::grace:
        settings.setValue(QLatin1String(remind_key), local_date(impl_->now));
        break;
    case lic::State::clock_suspect:
        impl_->clock_dismissed_for = notice_key(best);
        break;
    default:
        break;
    }
    settings.sync();
    Q_EMIT changed();
}

// ---- import and remove -----------------------------------------------------------------------

QVariantMap LicenceModel::importResult() const
{
    return impl_->import_result;
}

void LicenceModel::setImportResult(const QString& kind, const QString& title, const QString& text)
{
    QVariantMap result;
    result.insert(QStringLiteral("kind"), kind);
    result.insert(QStringLiteral("title"), title);
    result.insert(QStringLiteral("text"), text);
    impl_->import_result = result;
    Q_EMIT importResultChanged();
}

void LicenceModel::importFile(const QUrl& file)
{
    const LicenceEnvironment& env = impl_->env;
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QString failed_title = tr("Could not import the licence");

    QFile source(path);
    if (env.files == nullptr || !source.open(QIODevice::ReadOnly))
    {
        setImportResult(
            QStringLiteral("error"), failed_title,
            tr("ARINC Data Loader could not read %1. Check that the file still exists and that you are allowed "
               "to open it.")
                .arg(QDir::toNativeSeparators(path)));
        return;
    }
    // One byte more than a licence file may hold, so that a larger file is refused as too large.
    const QByteArray bytes = source.read(static_cast<qint64>(lic::signed_file::max_file_size) + 1);

    const lic::ClockState clock = store::load_clock(*env.files, env.folders);
    lic::Context where;
    where.now = env.now ? env.now() : lic::TimePoint{};
    where.latest_seen = clock.latest_seen;
    where.build_time = env.build_time;
    where.computer_name = env.computer_name;
    where.keys = env.keys;
    const auto imported = store::import_file(
        *env.files, env.folders, std::string_view{bytes.constData(), static_cast<std::size_t>(bytes.size())}, where);

    if (!imported)
    {
        switch (imported.error().failure)
        {
        case store::ImportFailure::refused:
            setImportResult(QStringLiteral("error"), failed_title,
                            invalid_sentence(imported.error().reason, env.keys.empty(), text_of(env.computer_name)));
            break;
        case store::ImportFailure::folder_full:
            setImportResult(QStringLiteral("error"), failed_title,
                            tr("The licence folder already holds %1 files. Remove one, then import again.")
                                .arg(store::max_files_per_folder));
            break;
        case store::ImportFailure::write_failed:
            setImportResult(
                QStringLiteral("error"), failed_title,
                tr("The licence could not be saved in %1. Check that the folder can be written.").arg(userFolder()));
            break;
        }
        return;
    }

    // A licence that carries a clock reset puts the stored time right, once, and is then judged by that time.
    lic::Status status = imported->status;
    if (status.value)
    {
        const lic::ClockState reset = lic::apply_reset(clock, *status.value);
        if (!(reset == clock))
        {
            static_cast<void>(store::save_clock(*env.files, env.folders, reset));
            where.latest_seen = reset.latest_seen;
            status = lic::evaluate(*status.value, nullptr, where);
        }
    }
    judge();
    Q_EMIT changed();

    const QString title = tr("Licence imported");
    if (imported->revocation_list)
    {
        setImportResult(QStringLiteral("ok"), tr("List of withdrawn licences imported"),
                        tr("The list was imported. A licence on it is shown as withdrawn."));
    }
    else if (imported->already_there)
    {
        setImportResult(QStringLiteral("info"), title, tr("That licence is already installed."));
    }
    else if (status.state == lic::State::expired)
    {
        setImportResult(QStringLiteral("warn"), title,
                        tr("This licence ended on %1. It was imported, but it includes nothing now.")
                            .arg(date_text(status.value->expires_at)));
    }
    else if (status.state == lic::State::grace)
    {
        setImportResult(QStringLiteral("warn"), title,
                        tr("This licence ended on %1. It was imported, and its grace period lasts until %2.")
                            .arg(date_text(status.value->expires_at), date_text(status.grace_ends)));
    }
    else if (status.state == lic::State::not_yet_valid)
    {
        setImportResult(QStringLiteral("info"), title,
                        tr("This licence starts on %1.").arg(date_text(status.value->starts_at)));
    }
    else if (status.state == lic::State::clock_suspect)
    {
        setImportResult(
            QStringLiteral("warn"), title,
            tr("The date on this computer looks wrong, so the licence cannot be judged yet. It was imported."));
    }
    else
    {
        setImportResult(QStringLiteral("ok"), title, tr("This is now the %1 edition.").arg(editionText()));
    }
}

void LicenceModel::removeFile(const QString& name)
{
    const LicenceEnvironment& env = impl_->env;
    if (env.files == nullptr)
    {
        return;
    }
    const QByteArray utf8 = name.toUtf8();
    const auto removed = store::remove_file(*env.files, env.folders,
                                            std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())});
    if (!removed)
    {
        setImportResult(QStringLiteral("error"), tr("Could not remove the licence"),
                        tr("The file %1 could not be removed from %2.").arg(name, userFolder()));
        return;
    }
    impl_->import_result.clear();
    Q_EMIT importResultChanged();
    judge();
    Q_EMIT changed();
}

// ---- support ---------------------------------------------------------------------------------

namespace
{

enum class SupportKind
{
    included,
    ended,
    community,
};

[[nodiscard]] SupportKind support_kind(const lic::Status& best)
{
    if (lic::check(best, lic::feature_support).granted)
    {
        return SupportKind::included;
    }
    const bool lists_support =
        best.value && std::ranges::any_of(best.value->features, [](const std::string& entry)
                                          { return lic::feature_matches(entry, lic::feature_support); });
    return best.state == lic::State::expired && lists_support ? SupportKind::ended : SupportKind::community;
}

} // namespace

QString LicenceModel::supportLabel() const
{
    switch (support_kind(impl_->best))
    {
    case SupportKind::included:
        return tr("Support included");
    case SupportKind::ended:
        return tr("Support ended");
    case SupportKind::community:
        break;
    }
    return tr("Community support");
}

QString LicenceModel::supportTone() const
{
    switch (support_kind(impl_->best))
    {
    case SupportKind::included:
        return QStringLiteral("ok");
    case SupportKind::ended:
        return QStringLiteral("warn");
    case SupportKind::community:
        break;
    }
    return QStringLiteral("neutral");
}

QString LicenceModel::supportText() const
{
    const lic::Status& best = impl_->best;
    switch (support_kind(best))
    {
    case SupportKind::included:
    {
        const lic::TimePoint until = best.state == lic::State::grace ? best.grace_ends : best.value->expires_at;
        return tr("Your %1 licence includes support until %2. Write to the support address you were given with the "
                  "licence, and quote your licence number, %3.")
            .arg(editionText(), date_text(until), licenceNumber());
    }
    case SupportKind::ended:
        return tr("Support ended with your licence on %1.").arg(date_text(best.value->expires_at));
    case SupportKind::community:
        break;
    }
    return tr(
        "No licence that includes support is installed. Read the guide first, then open an issue on the project's "
        "GitHub page. The Community edition has no support promised; the maintainer is one person.");
}

QString LicenceModel::supportDetails() const
{
    QStringList lines;
    lines << tr("Version: %1").arg(QString::fromLatin1(kVersionFull));
    lines << tr("Edition: %1").arg(editionText());
    if (hasLicence())
    {
        lines << tr("Licence number: %1").arg(licenceNumber());
    }
    lines << tr("Built for: %1").arg(QString::fromLatin1(kBuildTarget));
    return lines.join(QLatin1Char('\n'));
}

QString LicenceModel::issuesAddress() const
{
    return QString::fromLatin1(issues_address);
}

QString LicenceModel::projectAddress() const
{
    return QString::fromLatin1(project_address);
}

void LicenceModel::copyText(const QString& text) const
{
    if (auto* clipboard = QGuiApplication::clipboard())
    {
        clipboard->setText(text);
    }
}

} // namespace adl::ui
