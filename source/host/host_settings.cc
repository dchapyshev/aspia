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
    : system_settings_(base::XmlSettings::Scope::SYSTEM, "aspia", "host"),
      user_settings_(qt_base::QtXmlSettings::format(),
                     QSettings::UserScope,
                     QLatin1String("aspia"),
                     QLatin1String("host"))
{
    // Nothing
}

Settings::~Settings() = default;

// static
bool Settings::importFromFile(const std::filesystem::path& path, bool silent, QWidget* parent)
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
bool Settings::exportToFile(const std::filesystem::path& path, bool silent, QWidget* parent)
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

const std::filesystem::path& Settings::filePath() const
{
    return system_settings_.filePath();
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
    return system_settings_.get<uint16_t>("TcpPort", DEFAULT_HOST_TCP_PORT);
}

void Settings::setTcpPort(uint16_t port)
{
    system_settings_.set<uint16_t>("TcpPort", port);
}

UserList Settings::userList() const
{
    UserList users;

    size_t count = system_settings_.beginReadArray("Users");
    for (size_t i = 0; i < count; ++i)
    {
        system_settings_.setArrayIndex(i);

        User user;

        user.name      = system_settings_.get<std::u16string>("Name");
        user.salt      = system_settings_.get<base::ByteArray>("Salt");
        user.verifier  = system_settings_.get<base::ByteArray>("Verifier");
        user.number    = system_settings_.get<base::ByteArray>("Number");
        user.generator = system_settings_.get<base::ByteArray>("Generator");
        user.sessions  = system_settings_.get<uint32_t>("Sessions");
        user.flags     = system_settings_.get<uint32_t>("Flags");

        if (!user.isValid())
        {
            LOG(LS_ERROR) << "The list of users is corrupted.";
            return UserList();
        }

        users.add(user);
    }
    system_settings_.endArray();

    base::ByteArray seed_key = system_settings_.get<base::ByteArray>("SeedKey");
    if (seed_key.empty())
        seed_key = crypto::Random::byteArray(64);

    users.setSeedKey(seed_key);

    return users;
}

void Settings::setUserList(const UserList& users)
{
    // Clear the old list of users.
    system_settings_.remove("Users");

    system_settings_.set("SeedKey", users.seedKey());

    system_settings_.beginWriteArray("Users");
    for (size_t i = 0; i < users.count(); ++i)
    {
        system_settings_.setArrayIndex(i);

        const User& user = users.at(i);

        system_settings_.set("Name", user.name);
        system_settings_.set("Salt", user.salt);
        system_settings_.set("Verifier", user.verifier);
        system_settings_.set("Number", user.number);
        system_settings_.set("Generator", user.generator);
        system_settings_.set("Sessions", user.sessions);
        system_settings_.set("Flags", user.flags);
    }
    system_settings_.endArray();
}

std::string Settings::updateServer() const
{
    return system_settings_.get<std::string>("UpdateServer", DEFAULT_UPDATE_SERVER);
}

void Settings::setUpdateServer(const std::string& server)
{
    system_settings_.set("UpdateServer", server);
}

// static
bool Settings::copySettings(const std::filesystem::path& source_path,
                            const std::filesystem::path& target_path,
                            bool silent,
                            QWidget* parent)
{
    base::XmlSettings::Map settings_map;

    if (!base::XmlSettings::readFile(source_path, settings_map))
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

    if (!base::XmlSettings::writeFile(target_path, settings_map))
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
