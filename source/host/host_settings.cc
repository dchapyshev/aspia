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

    for (const auto& item : system_settings_.getArray("Users"))
    {
        User user;

        user.name      = item.get<std::u16string>("Name");
        user.salt      = item.get<base::ByteArray>("Salt");
        user.verifier  = item.get<base::ByteArray>("Verifier");
        user.number    = item.get<base::ByteArray>("Number");
        user.generator = item.get<base::ByteArray>("Generator");
        user.sessions  = item.get<uint32_t>("Sessions");
        user.flags     = item.get<uint32_t>("Flags");

        users.add(std::move(user));
    }

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

    base::Settings::Array users_array;

    for (size_t i = 0; i < users.count(); ++i)
    {
        const User& user = users.at(i);

        base::Settings item;
        item.set("Name", user.name);
        item.set("Salt", user.salt);
        item.set("Verifier", user.verifier);
        item.set("Number", user.number);
        item.set("Generator", user.generator);
        item.set("Sessions", user.sessions);
        item.set("Flags", user.flags);

        users_array.emplace_back(std::move(item));
    }

    system_settings_.setArray("Users", users_array);
    system_settings_.set("SeedKey", users.seedKey());
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
