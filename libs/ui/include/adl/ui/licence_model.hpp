// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the three programs say about the licence on this computer, as properties a window can bind to.
//
// A licence file is a record of what was bought. Nothing in a program is switched off by one: this model
// reads the licence files (adl::licence, over the store), says which edition they amount to and why, and
// words it for the person. The windows show those words and never read a file or decide a state.
//
// Everything a person reads is made here, translated when it is read, so a language switch (retranslate())
// changes the words the windows hold. The words follow the plan's vocabulary: licence is the noun, the
// editions are Community, Pro and Enterprise, a licence that has stopped is "ended", and no text says
// activate, register or unlock.
//
// The model keeps two small things between runs: which notice the person dismissed (the program's
// settings) and the latest time it has seen (the clock file of the store). It sends nothing anywhere.

#include "adl/licence/licence.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/store.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace adl::ui
{

// What the model needs of the computer, so that a test can give it any files, any time and any key.
struct LicenceEnvironment
{
    // Not owned; has to outlive the model.
    licence::store::Files* files = nullptr;
    licence::store::Folders folders;
    // The time now, UTC.
    std::function<licence::TimePoint()> now;
    // What a licence that names computers is compared with.
    std::string computer_name;
    // The keys a licence is checked against. The release's list is empty until the owner adds the public keys,
    // so on a release every licence is refused.
    std::vector<licence::signed_file::TrustedKey> keys;
    // When this program was built, or TimePoint::min() when that is not known: a time that has certainly passed.
    licence::TimePoint build_time = licence::TimePoint::min();
};

class LicenceModel : public QObject
{
    Q_OBJECT

    // The edition in force: "community", "pro" or "enterprise" (a word for a script), and the words for the person.
    Q_PROPERTY(QString edition READ edition NOTIFY changed)
    Q_PROPERTY(QString editionText READ editionText NOTIFY changed)
    // What the loader puts in its activity log and its receipt: "Edition: Community", or the edition and the number
    // of the licence in force ("Edition: Pro, licence ADL-2026-000123"). A licence that is not in force is not named.
    Q_PROPERTY(QString stamp READ stamp NOTIFY changed)
    // The state of the best licence: none, valid, expiring, grace, expired, not_yet_valid, clock_suspect or invalid.
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // The state as a chip says it, the tone of the chip (neutral, ok, warn or fail) and the sentence under it.
    Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY changed)
    Q_PROPERTY(QString stateTone READ stateTone NOTIFY changed)
    Q_PROPERTY(QString sentence READ sentence NOTIFY changed)
    // The edition chip of a window's status bar: its words, its tone and the name a screen reader says.
    Q_PROPERTY(QString chipText READ chipText NOTIFY changed)
    Q_PROPERTY(QString chipTone READ chipTone NOTIFY changed)
    Q_PROPERTY(QString chipName READ chipName NOTIFY changed)
    // The rows of the summary. A row with nothing to say is empty, and the window leaves it out.
    Q_PROPERTY(bool hasLicence READ hasLicence NOTIFY changed)
    Q_PROPERTY(QString licensee READ licensee NOTIFY changed)
    Q_PROPERTY(QString licenceNumber READ licenceNumber NOTIFY changed)
    Q_PROPERTY(QString term READ term NOTIFY changed)
    Q_PROPERTY(QString seats READ seats NOTIFY changed)
    Q_PROPERTY(QString computers READ computers NOTIFY changed)
    // What the licence includes and what it does not: { "name", "available", "why" } for each feature.
    Q_PROPERTY(QVariantList features READ features NOTIFY changed)
    // Every licence file found: { "name", "licenceNumber", "edition", "term", "stateLabel", "tone", "inUse",
    // "byAdministrator" }. A window shows the list when there is more than one.
    Q_PROPERTY(QVariantList files READ files NOTIFY changed)
    // Whether a licence file of the user's own is in use, so that the window offers to remove it.
    Q_PROPERTY(bool canRemove READ canRemove NOTIFY changed)
    // The name of the file in use in the user's folder ("" when there is none).
    Q_PROPERTY(QString fileInUse READ fileInUse NOTIFY changed)
    // Where licence files go: the user's folder, and the folder an administrator keeps.
    Q_PROPERTY(QString userFolder READ userFolder NOTIFY changed)
    Q_PROPERTY(QString machineFolder READ machineFolder NOTIFY changed)
    Q_PROPERTY(QString computerName READ computerName NOTIFY changed)
    // The notice a window shows under its header: its kind ("" for none, "info" or "warn"), its words and the
    // label of the button that puts it away.
    Q_PROPERTY(QString noticeKind READ noticeKind NOTIFY changed)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY changed)
    Q_PROPERTY(QString noticeDismissText READ noticeDismissText NOTIFY changed)
    // The last import: { "kind", "title", "text" }, where kind is "ok", "info", "warn" or "error", or empty.
    Q_PROPERTY(QVariantMap importResult READ importResult NOTIFY importResultChanged)
    // The Support dialog: the chip, the card's words and the details a helper asks for first.
    Q_PROPERTY(QString supportLabel READ supportLabel NOTIFY changed)
    Q_PROPERTY(QString supportTone READ supportTone NOTIFY changed)
    Q_PROPERTY(QString supportText READ supportText NOTIFY changed)
    Q_PROPERTY(QString supportDetails READ supportDetails NOTIFY changed)
    // The one address, and the one text, that the Support dialog copies: nothing opens a browser or a mail program.
    Q_PROPERTY(QString issuesAddress READ issuesAddress CONSTANT)
    Q_PROPERTY(QString projectAddress READ projectAddress CONSTANT)

public:
    // The model of this computer: the licence folders of the platform (moved by ADL_SETTINGS_DIR), the platform's
    // file system, its clock and name, and the release's keys.
    explicit LicenceModel(QObject* parent = nullptr);
    // A model over what the caller gives.
    explicit LicenceModel(LicenceEnvironment environment, QObject* parent = nullptr);
    ~LicenceModel() override;

    [[nodiscard]] QString edition() const;
    [[nodiscard]] QString editionText() const;
    [[nodiscard]] QString stamp() const;
    [[nodiscard]] QString state() const;
    [[nodiscard]] QString stateLabel() const;
    [[nodiscard]] QString stateTone() const;
    [[nodiscard]] QString sentence() const;
    [[nodiscard]] QString chipText() const;
    [[nodiscard]] QString chipTone() const;
    [[nodiscard]] QString chipName() const;
    [[nodiscard]] bool hasLicence() const;
    [[nodiscard]] QString licensee() const;
    [[nodiscard]] QString licenceNumber() const;
    [[nodiscard]] QString term() const;
    [[nodiscard]] QString seats() const;
    [[nodiscard]] QString computers() const;
    [[nodiscard]] QVariantList features() const;
    [[nodiscard]] QVariantList files() const;
    [[nodiscard]] bool canRemove() const;
    [[nodiscard]] QString fileInUse() const;
    [[nodiscard]] QString userFolder() const;
    [[nodiscard]] QString machineFolder() const;
    [[nodiscard]] QString computerName() const;
    [[nodiscard]] QString noticeKind() const;
    [[nodiscard]] QString noticeText() const;
    [[nodiscard]] QString noticeDismissText() const;
    [[nodiscard]] QVariantMap importResult() const;
    [[nodiscard]] QString supportLabel() const;
    [[nodiscard]] QString supportTone() const;
    [[nodiscard]] QString supportText() const;
    [[nodiscard]] QString supportDetails() const;
    [[nodiscard]] QString issuesAddress() const;
    [[nodiscard]] QString projectAddress() const;

    // Reads the files again, judges them, and keeps the clock file up to date. Called at start and by the
    // window when the licence dialog opens; the model also does it by itself once an hour.
    Q_INVOKABLE void refresh();

    // Copies a licence file into the user's folder, if it is a licence this program could use. The result is
    // importResult. Nothing is changed for a file that is refused, and a file already installed is not copied again.
    Q_INVOKABLE void importFile(const QUrl& file);

    // Removes one licence file from the user's folder, by the name `files` gave. Never one an administrator keeps.
    Q_INVOKABLE void removeFile(const QString& name);

    // Puts the notice away: until the licence changes for an expiring or an ended one, until tomorrow for one in
    // its grace period.
    Q_INVOKABLE void dismissNotice();

    // Puts text on the clipboard. The one place a window copies, so that the Support dialog's buttons are plain.
    Q_INVOKABLE void copyText(const QString& text) const;

    // The words changed language: every property that holds words announces itself again.
    Q_INVOKABLE void retranslate();

Q_SIGNALS:
    void changed();
    void importResultChanged();

private:
    struct Impl;

    void judge();
    void setImportResult(const QString& kind, const QString& title, const QString& text);

    std::unique_ptr<Impl> impl_;
    QTimer hourly_;
};

} // namespace adl::ui
