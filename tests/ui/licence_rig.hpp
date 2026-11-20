// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A LicenceModel over licence files held in memory, which a QML test puts into any state with one call. It is
// the `licence` of the Qt Quick Test suite's windows, and `licenceRig` is how a test file changes it: the
// test key is the only key it trusts, and the time is its own.

#include "adl/ui/licence_model.hpp"
#include "licence_fixtures.hpp"
#include "memory_files.hpp"

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>
#include <memory>
#include <string>

namespace adl::ui_test
{

class LicenceRig : public QObject
{
    Q_OBJECT

public:
    explicit LicenceRig(QObject* parent = nullptr) : QObject(parent)
    {
        adl::ui::LicenceEnvironment environment;
        environment.files = &files_;
        environment.folders = {"/home/pat/licences", "/etc/licences", "/home/pat/licence-clock.json"};
        environment.now = [this] { return now_; };
        environment.computer_name = "LAB-RIG-07";
        const auto keys = lic::test_keys::trusted();
        environment.keys.assign(keys.begin(), keys.end());
        model_ = std::make_unique<adl::ui::LicenceModel>(std::move(environment));
    }

    [[nodiscard]] adl::ui::LicenceModel& model() noexcept
    {
        return *model_;
    }

    // Puts the licence files and the time into one of the states: none, valid, expiring, grace, expired,
    // not_yet_valid, clock_suspect, wrong_computer, damaged or two_files.
    Q_INVOKABLE void use(const QString& state)
    {
        files_.files.clear();
        now_ = at(2027, 1, 10, 12);
        lic::Licence value = pro();
        if (state == QLatin1String("valid"))
        {
            put("user/a.adllic", value);
        }
        else if (state == QLatin1String("expiring"))
        {
            now_ = at(2027, 11, 3, 12);
            put("user/a.adllic", value);
        }
        else if (state == QLatin1String("grace"))
        {
            now_ = at(2027, 11, 20, 12);
            put("user/a.adllic", value);
        }
        else if (state == QLatin1String("expired"))
        {
            now_ = at(2028, 2, 1, 12);
            put("user/a.adllic", value);
        }
        else if (state == QLatin1String("not_yet_valid"))
        {
            value.starts_at = at(2027, 3, 1);
            value.expires_at = at(2028, 3, 1);
            value.issued_at = at(2027, 1, 1);
            put("user/a.adllic", value);
        }
        else if (state == QLatin1String("clock_suspect"))
        {
            put("user/a.adllic", value);
            model_->refresh();
            now_ = at(2026, 12, 10, 12);
        }
        else if (state == QLatin1String("wrong_computer"))
        {
            value.machines = {"OTHER-*"};
            put("user/a.adllic", value);
        }
        else if (state == QLatin1String("damaged"))
        {
            files_.put("/home/pat/licences/a.adllic", "this is not a licence");
        }
        else if (state == QLatin1String("two_files"))
        {
            lic::Licence ended = pro();
            ended.licence_id = "ADL-2025-000001";
            ended.starts_at = at(2025, 1, 1);
            ended.expires_at = at(2026, 1, 1);
            ended.issued_at = at(2025, 1, 1);
            put("user/ended.adllic", ended);
            put("machine/site.adllic", value);
        }
        model_->refresh();
    }

    // A licence file on disk, to import: "valid" or "damaged".
    Q_INVOKABLE QUrl licenceFile(const QString& kind)
    {
        QFile file(directory_.filePath(kind + QStringLiteral(".adllic")));
        if (!file.open(QIODevice::WriteOnly))
        {
            qFatal("the test file cannot be written");
        }
        const std::string bytes =
            kind == QLatin1String("valid") ? signed_file_of(pro()) : std::string{"this is not a licence"};
        file.write(bytes.data(), static_cast<qint64>(bytes.size()));
        file.close();
        return QUrl::fromLocalFile(file.fileName());
    }

    [[nodiscard]] Q_INVOKABLE QString clipboardText() const
    {
        return QGuiApplication::clipboard()->text();
    }

    [[nodiscard]] Q_INVOKABLE int filesInUserFolder() const
    {
        return static_cast<int>(files_.names_in("/home/pat/licences").size());
    }

private:
    void put(const std::string& where, const lic::Licence& value)
    {
        const std::string folder = where.starts_with("machine/") ? "/etc/licences/" : "/home/pat/licences/";
        files_.put(folder + where.substr(where.find('/') + 1), signed_file_of(value));
    }

    adl::test::MemoryFiles files_;
    lic::TimePoint now_ = at(2027, 1, 10, 12);
    std::unique_ptr<adl::ui::LicenceModel> model_;
    QTemporaryDir directory_;
};

} // namespace adl::ui_test
