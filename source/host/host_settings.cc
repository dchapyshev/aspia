//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/host_settings.h"

#include <QFile>
#include <QLocale>
#include <QMessageBox>

#include "base/logging.h"
#include "base/xml_settings.h"
#include "build/build_config.h"
#include "crypto/random.h"
#include "net/srp_user.h"

namespace host {

Settings::Settings()
    : settings_(base::XmlSettings::format(),
                QSettings::SystemScope,
                QLatin1String("aspia"),
                QLatin1String("host"))
{
    // Nothing
}

Settings::~Settings() = default;

// static
bool Settings::importFromFile(const QString& path, bool silent, QWidget* parent)
{
    bool result = copySettings(path, Settings().filePath(), silent, parent);

    if (!silent && result)
    {
        QMessageBox::information(parent,
                                 tr("Information"),
                                 tr("The configuration was successfully imported."),
                                 QMessageBox::Ok);
    }

    return result;
}

// static
bool Settings::exportToFile(const QString& path, bool silent, QWidget* parent)
{
    bool result = copySettings(Settings().filePath(), path, silent, parent);

    if (!silent && result)
    {
        QMessageBox::information(parent,
                                 tr("Information"),
                                 tr("The configuration was successfully exported."),
                                 QMessageBox::Ok);
    }

    return result;
}

QString Settings::filePath() const
{
    return settings_.fileName();
}

bool Settings::isWritable() const
{
    return settings_.isWritable();
}

QString Settings::locale() const
{
    return settings_.value(QStringLiteral("Locale"), QLocale::system().bcp47Name()).toString();
}

void Settings::setLocale(const QString& locale)
{
    settings_.setValue(QStringLiteral("Locale"), locale);
}

uint16_t Settings::tcpPort() const
{
    return settings_.value(QStringLiteral("TcpPort"), DEFAULT_HOST_TCP_PORT).toUInt();
}

void Settings::setTcpPort(uint16_t port)
{
    settings_.setValue(QStringLiteral("TcpPort"), port);
}

bool Settings::addFirewallRule() const
{
    return settings_.value(QStringLiteral("AddFirewallRule"), true).toBool();
}

void Settings::setAddFirewallRule(bool value)
{
    settings_.setValue(QStringLiteral("AddFirewallRule"), value);
}

net::SrpUserList Settings::userList() const
{
    net::SrpUserList users;

    users.seed_key = settings_.value(QStringLiteral("SeedKey")).toByteArray();
    if (users.seed_key.isEmpty())
        users.seed_key = crypto::Random::generateBuffer(64);

    int count = settings_.beginReadArray(QStringLiteral("Users"));
    for (int i = 0; i < count; ++i)
    {
        settings_.setArrayIndex(i);

        net::SrpUser user;
        user.name      = settings_.value(QStringLiteral("Name")).toString();
        user.salt      = settings_.value(QStringLiteral("Salt")).toByteArray();
        user.verifier  = settings_.value(QStringLiteral("Verifier")).toByteArray();
        user.number    = settings_.value(QStringLiteral("Number")).toByteArray();
        user.generator = settings_.value(QStringLiteral("Generator")).toByteArray();
        user.sessions  = settings_.value(QStringLiteral("Sessions")).toUInt();
        user.flags     = settings_.value(QStringLiteral("Flags")).toUInt();

        if (user.name.isEmpty() || user.salt.isEmpty() || user.verifier.isEmpty() ||
            user.number.isEmpty() || user.generator.isEmpty())
        {
            LOG(LS_ERROR) << "The list of users is corrupted.";
            return net::SrpUserList();
        }

        users.list.append(user);
    }
    settings_.endArray();

    return users;
}

void Settings::setUserList(const net::SrpUserList& users)
{
    // Clear the old list of users.
    settings_.remove(QStringLiteral("Users"));

    settings_.setValue(QStringLiteral("SeedKey"), users.seed_key);

    settings_.beginWriteArray(QStringLiteral("Users"));
    for (int i = 0; i < users.list.size(); ++i)
    {
        settings_.setArrayIndex(i);

        const net::SrpUser& user = users.list.at(i);

        settings_.setValue(QStringLiteral("Name"), user.name);
        settings_.setValue(QStringLiteral("Salt"), user.salt);
        settings_.setValue(QStringLiteral("Verifier"), user.verifier);
        settings_.setValue(QStringLiteral("Number"), user.number);
        settings_.setValue(QStringLiteral("Generator"), user.generator);
        settings_.setValue(QStringLiteral("Sessions"), user.sessions);
        settings_.setValue(QStringLiteral("Flags"), user.flags);
    }
    settings_.endArray();
}

QString Settings::updateServer() const
{
    return settings_.value(QStringLiteral("UpdateServer"), DEFAULT_UPDATE_SERVER).toString();
}

void Settings::setUpdateServer(const QString& server)
{
    settings_.setValue(QStringLiteral("UpdateServer"), server);
}

// static
bool Settings::copySettings(
    const QString& source_path, const QString& target_path, bool silent, QWidget* parent)
{
    QFile source_file(source_path);

    if (!source_file.open(QIODevice::ReadOnly))
    {
        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Could not open source file: %1")
                                 .arg(source_file.errorString()),
                                 QMessageBox::Ok);
        }

        return false;
    }

    QFile target_file(target_path);

    if (!target_file.open(QIODevice::WriteOnly))
    {
        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Could not open target file: %1")
                                 .arg(target_file.errorString()),
                                 QMessageBox::Ok);
        }

        return false;
    }

    QSettings::SettingsMap settings_map;

    if (!base::XmlSettings::readFunc(source_file, settings_map))
    {
        if (!silent)
        {
            QMessageBox::warning(
                parent,
                tr("Warning"),
                tr("Unable to read the source file: the file is damaged or has an unknown format."),
                QMessageBox::Ok);
        }

        return false;
    }

    if (!base::XmlSettings::writeFunc(target_file, settings_map))
    {
        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Unable to write the target file."),
                                 QMessageBox::Ok);
        }

        return false;
    }

    return true;
}

} // namespace host
