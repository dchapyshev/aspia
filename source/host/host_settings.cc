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

#include <QLocale>

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

        CHECK(!user.name.isEmpty());
        CHECK(!user.salt.isEmpty());
        CHECK(!user.verifier.isEmpty());
        CHECK(!user.number.isEmpty());
        CHECK(!user.generator.isEmpty());

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

} // namespace host
