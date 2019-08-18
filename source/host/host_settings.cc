//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "build/build_config.h"
#include "crypto/random.h"
#include "host/user.h"
#include "qt_base/qt_xml_settings.h"

#include <QFile>
#include <QLocale>
#include <QMessageBox>

namespace host {

Settings::Settings()
    : system_settings_(qt_base::QtXmlSettings::format(),
                       QSettings::SystemScope,
                       QLatin1String("aspia"),
                       QLatin1String("host")),
      user_settings_(qt_base::QtXmlSettings::format(),
                     QSettings::UserScope,
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
    return system_settings_.fileName();
}

bool Settings::isWritable() const
{
    return system_settings_.isWritable();
}

void Settings::sync()
{
    system_settings_.sync();
    user_settings_.sync();
}

QString Settings::locale() const
{
    return user_settings_.value(
        QStringLiteral("Locale"), QLocale::system().bcp47Name()).toString();
}

void Settings::setLocale(const QString& locale)
{
    user_settings_.setValue(QStringLiteral("Locale"), locale);
}

uint16_t Settings::tcpPort() const
{
    return system_settings_.value(QStringLiteral("TcpPort"), DEFAULT_HOST_TCP_PORT).toUInt();
}

void Settings::setTcpPort(uint16_t port)
{
    system_settings_.setValue(QStringLiteral("TcpPort"), port);
}

UserList Settings::userList() const
{
    UserList users;

    int count = system_settings_.beginReadArray(QStringLiteral("Users"));
    for (int i = 0; i < count; ++i)
    {
        system_settings_.setArrayIndex(i);

        User user;
        user.name      = system_settings_.value(QStringLiteral("Name")).toString().toStdU16String();
        user.salt      = system_settings_.value(QStringLiteral("Salt")).toByteArray().toStdString();
        user.verifier  = system_settings_.value(QStringLiteral("Verifier")).toByteArray().toStdString();
        user.number    = system_settings_.value(QStringLiteral("Number")).toByteArray().toStdString();
        user.generator = system_settings_.value(QStringLiteral("Generator")).toByteArray().toStdString();
        user.sessions  = system_settings_.value(QStringLiteral("Sessions")).toUInt();
        user.flags     = system_settings_.value(QStringLiteral("Flags")).toUInt();

        if (!user.isValid())
        {
            LOG(LS_ERROR) << "The list of users is corrupted.";
            return UserList();
        }

        users.add(user);
    }
    system_settings_.endArray();

    std::string seed_key = system_settings_.value(QStringLiteral("SeedKey")).toByteArray().toStdString();
    if (seed_key.empty())
        seed_key = crypto::Random::string(64);

    users.setSeedKey(seed_key);

    return users;
}

void Settings::setUserList(const UserList& users)
{
    // Clear the old list of users.
    system_settings_.remove(QStringLiteral("Users"));

    system_settings_.setValue(QStringLiteral("SeedKey"), QByteArray::fromStdString(users.seedKey()));

    system_settings_.beginWriteArray(QStringLiteral("Users"));
    for (int i = 0; i < users.count(); ++i)
    {
        system_settings_.setArrayIndex(i);

        const User& user = users.at(i);

        system_settings_.setValue(QStringLiteral("Name"), QString::fromStdU16String(user.name));
        system_settings_.setValue(QStringLiteral("Salt"), QByteArray::fromStdString(user.salt));
        system_settings_.setValue(QStringLiteral("Verifier"), QByteArray::fromStdString(user.verifier));
        system_settings_.setValue(QStringLiteral("Number"), QByteArray::fromStdString(user.number));
        system_settings_.setValue(QStringLiteral("Generator"), QByteArray::fromStdString(user.generator));
        system_settings_.setValue(QStringLiteral("Sessions"), user.sessions);
        system_settings_.setValue(QStringLiteral("Flags"), user.flags);
    }
    system_settings_.endArray();
}

QString Settings::updateServer() const
{
    return system_settings_.value(
        QStringLiteral("UpdateServer"), DEFAULT_UPDATE_SERVER).toString();
}

void Settings::setUpdateServer(const QString& server)
{
    system_settings_.setValue(QStringLiteral("UpdateServer"), server);
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

    if (!qt_base::QtXmlSettings::readFunc(source_file, settings_map))
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

    if (!qt_base::QtXmlSettings::writeFunc(target_file, settings_map))
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
