// SPDX-License-Identifier: GPL-3.0-or-later
// What the licence model says (libs/ui/licence_model.cpp): for each state a licence can be in, the edition, the
// chip, the sentence, what the licence includes, the notice and what a person can do about it; importing and
// removing a licence file; what is remembered between runs. The files are held in memory, the keys are the
// published test key of RFC 8032 and the time is the test's own, so every state is reached by writing a file.
// No translation is installed, so the words are the English source; a number of days is "%n day(s)" with the
// number filled in.

#include "adl/licence/licence.hpp"
#include "adl/licence/product.hpp"
#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/test_keys.hpp"
#include "adl/ui/licence_model.hpp"
#include "licence_fixtures.hpp"
#include "memory_files.hpp"
#include "scratch_settings.hpp"

#include <QDate>
#include <QDateTime>
#include <QGuiApplication>
#include <QLocale>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QUrl>
#include <chrono>
#include <memory>
#include <span>
#include <string>

namespace lic = adl::licence;
using adl::test::MemoryFiles;
using adl::ui_test::at;
using adl::ui_test::pro;
using adl::ui_test::signed_file_of;

namespace
{

// The date a person sees: local, long form.
QString shown(lic::TimePoint time)
{
    const QDateTime utc = QDateTime::fromSecsSinceEpoch(time.time_since_epoch().count(), QTimeZone::UTC);
    return QLocale().toString(utc.toLocalTime().date(), QLocale::LongFormat);
}

const lic::store::Folders folders{"/home/pat/licences", "/etc/licences", "/home/pat/licence-clock.json"};

// A model over files in memory, at a time the test moves.
struct Rig
{
    MemoryFiles files;
    lic::TimePoint now = at(2027, 1, 10, 12);
    std::unique_ptr<adl::ui::LicenceModel> model;

    explicit Rig(bool trusted = true)
    {
        adl::ui::LicenceEnvironment environment;
        environment.files = &files;
        environment.folders = folders;
        environment.now = [this] { return now; };
        environment.computer_name = "LAB-RIG-07";
        if (trusted)
        {
            const auto keys = lic::test_keys::trusted();
            environment.keys.assign(keys.begin(), keys.end());
        }
        model = std::make_unique<adl::ui::LicenceModel>(std::move(environment));
    }

    void put_user(const std::string& name, const lic::Licence& value)
    {
        files.put(folders.user + "/" + name, signed_file_of(value));
    }
    void put_machine(const std::string& name, const lic::Licence& value)
    {
        files.put(folders.machine + "/" + name, signed_file_of(value));
    }
};

QString text(const QVariantMap& row, const char* key)
{
    return row.value(QLatin1String(key)).toString();
}

} // namespace

class LicenceModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        adl::test::scratch_settings().next();
    }

    void no_licence_is_the_community_edition()
    {
        Rig rig;
        auto& model = *rig.model;
        QCOMPARE(model.edition(), QStringLiteral("community"));
        QCOMPARE(model.editionText(), QStringLiteral("Community"));
        QCOMPARE(model.state(), QStringLiteral("none"));
        QCOMPARE(model.chipText(), QStringLiteral("Community"));
        QCOMPARE(model.chipTone(), QStringLiteral("neutral"));
        QVERIFY(model.chipName().contains(QStringLiteral("Edition: Community")));
        QVERIFY(model.sentence().startsWith(QStringLiteral("No licence is installed.")));
        QVERIFY(!model.hasLicence());
        QVERIFY(model.licensee().isEmpty());
        QVERIFY(model.term().isEmpty());
        QVERIFY(model.noticeKind().isEmpty());
        QVERIFY(model.files().isEmpty());
        QVERIFY(!model.canRemove());
        // Nothing is written for a computer that has no licence: not even the clock file.
        QVERIFY(!rig.files.has(folders.clock_file));
        // What a licence would add, and what it needs.
        const QVariantList features = model.features();
        QCOMPARE(features.size(), 2);
        QCOMPARE(text(features[0].toMap(), "name"), QStringLiteral("Support"));
        QVERIFY(!features[0].toMap().value(QStringLiteral("available")).toBool());
        QCOMPARE(text(features[0].toMap(), "why"), QStringLiteral("Needs a Pro or Enterprise licence."));
        QCOMPARE(text(features[1].toMap(), "why"), QStringLiteral("Needs an Enterprise licence."));
    }

    void a_valid_licence_says_whose_it_is_and_what_it_includes()
    {
        Rig rig;
        rig.put_user("ADL-2026-000123.adllic", pro());
        rig.model->refresh();
        auto& model = *rig.model;
        QCOMPARE(model.edition(), QStringLiteral("pro"));
        QCOMPARE(model.editionText(), QStringLiteral("Pro"));
        QCOMPARE(model.state(), QStringLiteral("valid"));
        QCOMPARE(model.stateLabel(), QStringLiteral("Valid"));
        QCOMPARE(model.stateTone(), QStringLiteral("ok"));
        QCOMPARE(model.chipText(), QStringLiteral("Pro"));
        QCOMPARE(model.chipTone(), QStringLiteral("ok"));
        QCOMPARE(model.sentence(), QStringLiteral("Valid until %1.").arg(shown(at(2027, 11, 13))));
        QVERIFY(model.hasLicence());
        QCOMPARE(model.licensee(), QStringLiteral("Example Lab Pty Ltd"));
        QCOMPARE(model.licenceNumber(), QStringLiteral("ADL-2026-000123"));
        QCOMPARE(model.term(), QStringLiteral("%1 to %2").arg(shown(at(2026, 11, 13)), shown(at(2027, 11, 13))));
        QVERIFY(model.seats().startsWith(QStringLiteral("3 seat")));
        QVERIFY(model.computers().isEmpty());
        QVERIFY(model.noticeKind().isEmpty());
        QVERIFY(model.canRemove());
        QCOMPARE(model.fileInUse(), QStringLiteral("ADL-2026-000123.adllic"));

        const QVariantList features = model.features();
        QVERIFY(features[0].toMap().value(QStringLiteral("available")).toBool());
        QVERIFY(!features[1].toMap().value(QStringLiteral("available")).toBool());
        QCOMPARE(text(features[1].toMap(), "why"), QStringLiteral("Your licence does not include this."));
        // The clock file now holds the time that was seen.
        QVERIFY(rig.files.has(folders.clock_file));
    }

    void a_licence_with_no_seat_count_is_a_site_licence()
    {
        Rig rig;
        auto value = pro();
        value.seats = 0;
        rig.put_user("a.adllic", value);
        rig.model->refresh();
        QCOMPARE(rig.model->seats(), QStringLiteral("Site licence"));
    }

    void a_licence_that_ends_soon_says_so_in_the_chip_and_a_notice()
    {
        Rig rig;
        rig.put_user("a.adllic", pro());
        rig.now = at(2027, 11, 3, 12);
        rig.model->refresh();
        auto& model = *rig.model;
        QCOMPARE(model.state(), QStringLiteral("expiring"));
        QCOMPARE(model.stateLabel(), QStringLiteral("Ends soon"));
        QCOMPARE(model.chipTone(), QStringLiteral("warn"));
        QVERIFY(model.chipText().startsWith(QStringLiteral("Pro, ")));
        QVERIFY(model.chipText().contains(QStringLiteral("9 day")));
        QVERIFY(model.sentence().contains(QStringLiteral("9 day")));
        QCOMPARE(model.noticeKind(), QStringLiteral("info"));
        QCOMPARE(model.noticeText(), QStringLiteral("Your licence ends on %1.").arg(shown(at(2027, 11, 13))));
        QCOMPARE(model.noticeDismissText(), QStringLiteral("Dismiss"));

        rig.now = at(2027, 11, 13, 0) - std::chrono::hours{1};
        rig.model->refresh();
        QCOMPARE(model.chipText(), QStringLiteral("Pro, ends today"));
        QVERIFY(model.sentence().endsWith(QStringLiteral("It ends today.")));
    }

    void a_dismissed_notice_stays_away_and_a_grace_notice_comes_back_tomorrow()
    {
        Rig rig;
        rig.put_user("a.adllic", pro());
        rig.now = at(2027, 11, 3, 12);
        rig.model->refresh();
        QCOMPARE(rig.model->noticeKind(), QStringLiteral("info"));
        QSignalSpy spy(rig.model.get(), &adl::ui::LicenceModel::changed);
        rig.model->dismissNotice();
        QVERIFY(spy.count() >= 1);
        QVERIFY(rig.model->noticeKind().isEmpty());
        // It is remembered, as it is for the next run.
        Rig again;
        again.put_user("a.adllic", pro());
        again.now = at(2027, 11, 3, 12);
        again.model->refresh();
        QVERIFY(again.model->noticeKind().isEmpty());

        // In the grace period the notice is a warning, "Remind me tomorrow" puts it away for the day.
        Rig grace;
        grace.put_user("a.adllic", pro());
        grace.now = at(2027, 11, 20, 12);
        grace.model->refresh();
        QCOMPARE(grace.model->state(), QStringLiteral("grace"));
        QCOMPARE(grace.model->noticeKind(), QStringLiteral("warn"));
        QCOMPARE(grace.model->noticeDismissText(), QStringLiteral("Remind me tomorrow"));
        QVERIFY(grace.model->noticeText().contains(shown(at(2027, 11, 13))));
        grace.model->dismissNotice();
        QVERIFY(grace.model->noticeKind().isEmpty());
        grace.now = at(2027, 11, 21, 12);
        grace.model->refresh();
        QCOMPARE(grace.model->noticeKind(), QStringLiteral("warn"));
    }

    void a_licence_in_its_grace_period_still_counts()
    {
        Rig rig;
        rig.put_user("a.adllic", pro());
        rig.now = at(2027, 11, 20, 12);
        rig.model->refresh();
        auto& model = *rig.model;
        QCOMPARE(model.edition(), QStringLiteral("pro"));
        QCOMPARE(model.chipText(), QStringLiteral("Pro, licence ended"));
        QCOMPARE(model.chipTone(), QStringLiteral("warn"));
        QCOMPARE(model.stateLabel(), QStringLiteral("Ended"));
        QCOMPARE(model.sentence(), QStringLiteral("Ended on %1. The grace period lasts until %2.")
                                       .arg(shown(at(2027, 11, 13)), shown(at(2027, 12, 11))));
        QVERIFY(model.features()[0].toMap().value(QStringLiteral("available")).toBool());
    }

    void an_ended_licence_returns_the_program_to_community_and_says_nothing_is_lost()
    {
        Rig rig;
        rig.put_user("a.adllic", pro());
        rig.now = at(2028, 2, 1, 12);
        rig.model->refresh();
        auto& model = *rig.model;
        QCOMPARE(model.state(), QStringLiteral("expired"));
        QCOMPARE(model.edition(), QStringLiteral("community"));
        QCOMPARE(model.chipText(), QStringLiteral("Community"));
        QCOMPARE(model.chipTone(), QStringLiteral("warn"));
        QVERIFY(model.sentence().contains(QStringLiteral("This is the Community edition again.")));
        QVERIFY(model.sentence().contains(QStringLiteral("Nothing you made is lost")));
        // The licence's own details are still shown, so that a person sees whose it was.
        QCOMPARE(model.licenceNumber(), QStringLiteral("ADL-2026-000123"));
        QCOMPARE(text(model.features()[0].toMap(), "why"),
                 QStringLiteral("Your licence ended on %1.").arg(shown(at(2027, 11, 13))));
        QCOMPARE(model.noticeKind(), QStringLiteral("info"));
        QVERIFY(model.noticeText().startsWith(QStringLiteral("Your licence has ended.")));
        model.dismissNotice();
        QVERIFY(model.noticeKind().isEmpty());
    }

    void a_licence_that_has_not_started_says_when_it_does()
    {
        Rig rig;
        auto value = pro();
        value.starts_at = at(2027, 3, 1);
        value.expires_at = at(2028, 3, 1);
        value.issued_at = at(2027, 1, 1);
        rig.put_user("a.adllic", value);
        rig.model->refresh();
        QCOMPARE(rig.model->state(), QStringLiteral("not_yet_valid"));
        QCOMPARE(rig.model->stateLabel(), QStringLiteral("Not started"));
        QCOMPARE(rig.model->sentence(), QStringLiteral("Starts on %1.").arg(shown(at(2027, 3, 1))));
        QCOMPARE(rig.model->chipText(), QStringLiteral("Community"));
        QCOMPARE(text(rig.model->features()[0].toMap(), "why"),
                 QStringLiteral("Your licence starts on %1.").arg(shown(at(2027, 3, 1))));
    }

    void a_clock_set_back_is_noticed_and_a_reset_licence_puts_it_right()
    {
        Rig rig;
        rig.put_user("a.adllic", pro());
        rig.model->refresh();
        QCOMPARE(rig.model->state(), QStringLiteral("valid"));
        // The clock goes back by a month.
        rig.now = at(2026, 12, 10, 12);
        rig.model->refresh();
        auto& model = *rig.model;
        QCOMPARE(model.state(), QStringLiteral("clock_suspect"));
        QCOMPARE(model.stateLabel(), QStringLiteral("Check the date"));
        QVERIFY(model.sentence().startsWith(QStringLiteral("The date on this computer looks wrong")));
        QCOMPARE(model.noticeKind(), QStringLiteral("warn"));
        QCOMPARE(text(model.features()[0].toMap(), "why"), QStringLiteral("The date on this computer looks wrong."));
        // Put away for this run only.
        model.dismissNotice();
        QVERIFY(model.noticeKind().isEmpty());

        // The owner sends a licence with a reset token: importing it sets the stored time back, once.
        QTemporaryDir directory;
        auto reset = pro();
        reset.licence_id = "ADL-2026-000124";
        reset.issued_at = at(2026, 12, 9);
        reset.starts_at = at(2026, 12, 9);
        reset.clock_reset = "reset-1";
        QFile file(directory.filePath(QStringLiteral("reset.adllic")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const std::string bytes = signed_file_of(reset);
        file.write(bytes.data(), static_cast<qint64>(bytes.size()));
        file.close();
        model.importFile(QUrl::fromLocalFile(file.fileName()));
        QCOMPARE(text(model.importResult(), "kind"), QStringLiteral("ok"));
        QCOMPARE(model.state(), QStringLiteral("valid"));
    }

    void a_licence_for_other_computers_and_for_another_product_and_a_damaged_file_are_not_valid()
    {
        Rig rig;
        auto elsewhere = pro();
        elsewhere.machines = {"OTHER-*"};
        rig.put_user("a.adllic", elsewhere);
        rig.model->refresh();
        auto& model = *rig.model;
        QCOMPARE(model.state(), QStringLiteral("invalid"));
        QCOMPARE(model.stateLabel(), QStringLiteral("Not valid"));
        QCOMPARE(model.stateTone(), QStringLiteral("fail"));
        QCOMPARE(model.sentence(),
                 QStringLiteral("This licence is for other computers. This computer is LAB-RIG-07. Ask "
                                "your administrator for a licence that names it."));
        QCOMPARE(text(model.features()[0].toMap(), "why"), QStringLiteral("Your licence is for other computers."));

        Rig product;
        auto other = pro();
        other.product = "vss";
        product.put_user("a.adllic", other);
        product.model->refresh();
        QVERIFY(product.model->sentence().startsWith(QStringLiteral("This licence is for another product.")));

        Rig damaged;
        damaged.files.put(folders.user + "/a.adllic", "this is not a licence");
        damaged.model->refresh();
        QVERIFY(damaged.model->sentence().startsWith(
            QStringLiteral("This file is not a valid ARINC Data Loader licence.")));
        QCOMPARE(text(damaged.model->features()[0].toMap(), "why"),
                 QStringLiteral("The installed licence is not valid."));

        Rig newer;
        newer.files.put(folders.user + "/a.adllic", signed_file_of(pro(), "lic-2099-a"));
        newer.model->refresh();
        QVERIFY(newer.model->sentence().startsWith(QStringLiteral("This licence needs a newer version")));
    }

    void a_revocation_list_withdraws_a_licence()
    {
        Rig rig;
        rig.put_user("a.adllic", pro());
        lic::RevocationList list;
        list.sequence = 1;
        list.issued_at = at(2027, 1, 1);
        list.licence_ids = {"ADL-2026-000123"};
        const auto payload = lic::write_revocations(list);
        QVERIFY(payload.has_value());
        const std::span<const std::uint8_t> bytes{reinterpret_cast<const std::uint8_t*>(payload->data()),
                                                  payload->size()};
        rig.files.put(folders.machine + "/list.adlrev",
                      lic::signed_file::write(lic::signing::sign(lic::revocations_purpose, lic::test_keys::key_id,
                                                                 bytes, lic::test_keys::private_key())));
        rig.model->refresh();
        QCOMPARE(rig.model->sentence(), QStringLiteral("This licence was withdrawn. Contact support."));
    }

    void a_build_with_no_keys_says_so_for_every_file()
    {
        Rig rig(false);
        rig.put_user("a.adllic", pro());
        rig.model->refresh();
        QCOMPARE(rig.model->state(), QStringLiteral("invalid"));
        QVERIFY(rig.model->sentence().startsWith(
            QStringLiteral("This version of ARINC Data Loader cannot check licence files yet")));
        // The programs are the Community edition, whatever files are there.
        QCOMPARE(rig.model->edition(), QStringLiteral("community"));
        QCOMPARE(rig.model->chipText(), QStringLiteral("Community"));
    }

    void the_best_of_several_files_is_in_use_and_an_administrators_file_cannot_be_removed()
    {
        Rig rig;
        rig.put_user("ended.adllic",
                     []
                     {
                         auto value = pro();
                         value.licence_id = "ADL-2025-000001";
                         value.starts_at = at(2025, 1, 1);
                         value.expires_at = at(2026, 1, 1);
                         value.issued_at = at(2025, 1, 1);
                         return value;
                     }());
        rig.put_machine("site.adllic", pro());
        rig.model->refresh();
        auto& model = *rig.model;
        const QVariantList files = model.files();
        QCOMPARE(files.size(), 2);
        QVERIFY(!files[0].toMap().value(QStringLiteral("inUse")).toBool());
        QVERIFY(files[1].toMap().value(QStringLiteral("inUse")).toBool());
        QVERIFY(files[1].toMap().value(QStringLiteral("byAdministrator")).toBool());
        QCOMPARE(text(files[0].toMap(), "stateLabel"), QStringLiteral("Ended"));
        QVERIFY(!model.canRemove());
        // A name that is only in the administrator's folder is refused.
        model.removeFile(QStringLiteral("site.adllic"));
        QCOMPARE(text(model.importResult(), "kind"), QStringLiteral("error"));
        QVERIFY(rig.files.has(folders.machine + "/site.adllic"));
        // The user's own can go.
        model.removeFile(QStringLiteral("ended.adllic"));
        QVERIFY(!rig.files.has(folders.user + "/ended.adllic"));
        QCOMPARE(model.files().size(), 1);
    }

    void importing_a_licence_copies_it_and_says_what_edition_it_is()
    {
        Rig rig;
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("licence.adllic"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const std::string bytes = signed_file_of(pro());
        file.write(bytes.data(), static_cast<qint64>(bytes.size()));
        file.close();

        auto& model = *rig.model;
        QSignalSpy imported(&model, &adl::ui::LicenceModel::importResultChanged);
        model.importFile(QUrl::fromLocalFile(path));
        QVERIFY(imported.count() >= 1);
        QCOMPARE(text(model.importResult(), "kind"), QStringLiteral("ok"));
        QCOMPARE(text(model.importResult(), "title"), QStringLiteral("Licence imported"));
        QCOMPARE(text(model.importResult(), "text"), QStringLiteral("This is now the Pro edition."));
        QCOMPARE(model.state(), QStringLiteral("valid"));
        QVERIFY(rig.files.has(folders.user + "/ADL-2026-000123.adllic"));
        // The original is left where it was.
        QVERIFY(QFile::exists(path));

        // The same licence again is not copied twice.
        model.importFile(QUrl::fromLocalFile(path));
        QCOMPARE(text(model.importResult(), "kind"), QStringLiteral("info"));
        QCOMPARE(text(model.importResult(), "text"), QStringLiteral("That licence is already installed."));
        QCOMPARE(model.files().size(), 1);
    }

    void importing_what_is_not_a_licence_changes_nothing_and_says_why()
    {
        Rig rig;
        rig.put_user("good.adllic", pro());
        rig.model->refresh();
        QTemporaryDir directory;
        QFile bad(directory.filePath(QStringLiteral("bad.adllic")));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("this is not a licence");
        bad.close();

        auto& model = *rig.model;
        model.importFile(QUrl::fromLocalFile(bad.fileName()));
        QCOMPARE(text(model.importResult(), "kind"), QStringLiteral("error"));
        QCOMPARE(text(model.importResult(), "title"), QStringLiteral("Could not import the licence"));
        QVERIFY(text(model.importResult(), "text")
                    .startsWith(QStringLiteral("This file is not a valid ARINC Data Loader licence")));
        QCOMPARE(model.files().size(), 1);
        QCOMPARE(model.state(), QStringLiteral("valid"));

        // A file that is not there.
        model.importFile(QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing.adllic"))));
        QCOMPARE(text(model.importResult(), "kind"), QStringLiteral("error"));
        QVERIFY(text(model.importResult(), "text").startsWith(QStringLiteral("ARINC Data Loader could not read")));
    }

    void importing_an_ended_or_a_future_licence_is_allowed_and_says_what_it_does()
    {
        Rig rig;
        QTemporaryDir directory;
        const auto write = [&](const QString& name, const lic::Licence& value)
        {
            QFile file(directory.filePath(name));
            if (!file.open(QIODevice::WriteOnly))
            {
                qFatal("the test file cannot be written");
            }
            const std::string bytes = signed_file_of(value);
            file.write(bytes.data(), static_cast<qint64>(bytes.size()));
            file.close();
            return QUrl::fromLocalFile(file.fileName());
        };
        auto ended = pro();
        ended.licence_id = "ADL-2025-000001";
        ended.starts_at = at(2025, 1, 1);
        ended.expires_at = at(2026, 1, 1);
        ended.issued_at = at(2025, 1, 1);
        ended.grace_days = 0;
        rig.model->importFile(write(QStringLiteral("ended.adllic"), ended));
        QCOMPARE(text(rig.model->importResult(), "kind"), QStringLiteral("warn"));
        QVERIFY(text(rig.model->importResult(), "text").contains(QStringLiteral("includes nothing now")));

        auto later = pro();
        later.licence_id = "ADL-2027-000002";
        later.starts_at = at(2027, 3, 1);
        later.expires_at = at(2028, 3, 1);
        later.issued_at = at(2027, 1, 1);
        rig.model->importFile(write(QStringLiteral("later.adllic"), later));
        QCOMPARE(text(rig.model->importResult(), "kind"), QStringLiteral("info"));
        QCOMPARE(text(rig.model->importResult(), "text"),
                 QStringLiteral("This licence starts on %1.").arg(shown(at(2027, 3, 1))));
    }

    void a_full_licence_folder_and_a_failing_disk_are_reported()
    {
        Rig rig;
        for (int i = 0; i < 64; ++i)
        {
            rig.files.put(folders.user + "/f" + std::to_string(100 + i) + ".adllic", "x");
        }
        QTemporaryDir directory;
        QFile file(directory.filePath(QStringLiteral("licence.adllic")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const std::string bytes = signed_file_of(pro());
        file.write(bytes.data(), static_cast<qint64>(bytes.size()));
        file.close();
        rig.model->importFile(QUrl::fromLocalFile(file.fileName()));
        QVERIFY(text(rig.model->importResult(), "text")
                    .startsWith(QStringLiteral("The licence folder already holds 64 files.")));

        Rig failing;
        failing.files.create_fails = adl::licence::store::FilesError::failed;
        failing.model->importFile(QUrl::fromLocalFile(file.fileName()));
        QVERIFY(text(failing.model->importResult(), "text")
                    .startsWith(QStringLiteral("The licence could not be saved in")));
    }

    void support_says_what_the_licence_includes()
    {
        Rig rig;
        QCOMPARE(rig.model->supportLabel(), QStringLiteral("Community support"));
        QCOMPARE(rig.model->supportTone(), QStringLiteral("neutral"));
        QVERIFY(rig.model->supportText().contains(QStringLiteral("no support promised")));

        rig.put_user("a.adllic", pro());
        rig.model->refresh();
        QCOMPARE(rig.model->supportLabel(), QStringLiteral("Support included"));
        QCOMPARE(rig.model->supportTone(), QStringLiteral("ok"));
        QCOMPARE(rig.model->supportText(),
                 QStringLiteral(
                     "Your Pro licence includes support until %1. Write to the support address you were given with the "
                     "licence, and quote your licence number, ADL-2026-000123.")
                     .arg(shown(at(2027, 11, 13))));
        QVERIFY(rig.model->supportDetails().contains(QStringLiteral("Licence number: ADL-2026-000123")));
        QVERIFY(rig.model->supportDetails().contains(QStringLiteral("Edition: Pro")));

        rig.now = at(2028, 2, 1, 12);
        rig.model->refresh();
        QCOMPARE(rig.model->supportLabel(), QStringLiteral("Support ended"));
        QCOMPARE(rig.model->supportTone(), QStringLiteral("warn"));
        QCOMPARE(rig.model->supportText(),
                 QStringLiteral("Support ended with your licence on %1.").arg(shown(at(2027, 11, 13))));
    }

    void nothing_in_the_words_says_activate_register_or_unlock()
    {
        // Every sentence a state can produce, for every state the tests above reach.
        const std::vector<std::pair<std::string, lic::TimePoint>> cases = {{"valid", at(2027, 1, 10)},
                                                                           {"expiring", at(2027, 11, 5)},
                                                                           {"grace", at(2027, 11, 20)},
                                                                           {"expired", at(2028, 2, 1)}};
        for (const auto& [name, when] : cases)
        {
            Rig rig;
            rig.put_user("a.adllic", pro());
            rig.now = when;
            rig.model->refresh();
            const QString all =
                rig.model->sentence() + rig.model->chipText() + rig.model->noticeText() + rig.model->supportText();
            for (const char* word : {"activat", "register", "unlock"})
            {
                QVERIFY2(!all.contains(QLatin1String(word), Qt::CaseInsensitive),
                         qPrintable(QString::fromStdString(name)));
            }
        }
    }

    void a_language_switch_announces_every_property_again()
    {
        Rig rig;
        QSignalSpy spy(rig.model.get(), &adl::ui::LicenceModel::changed);
        rig.model->retranslate();
        QCOMPARE(spy.count(), 1);
    }
};

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("adl-tests"));
    QGuiApplication::setApplicationName(QStringLiteral("adl_licence_model_tests"));
    adl::test::scratch_settings();
    LicenceModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "licence_model_test.moc"
