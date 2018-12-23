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

#include "base/logging.h"
#include "base/xml_settings.h"
#include "build/build_config.h"
#include "crypto/random.h"
#include "network/srp_user.h"

namespace aspia {

HostSettings::HostSettings()
    : settings_(XmlSettings::registerFormat(),
                QSettings::SystemScope,
                QLatin1String("aspia"),
                QLatin1String("host"))
{
    // Nothing
}

HostSettings::~HostSettings() = default;

QString HostSettings::filePath() const
{
    return settings_.fileName();
}

bool HostSettings::isWritable() const
{
    return settings_.isWritable();
}

QString HostSettings::locale() const
{
    return settings_.value(QLatin1String("Locale"), DEFAULT_LOCALE).toString();
}

void HostSettings::setLocale(const QString& locale)
{
    settings_.setValue(QLatin1String("Locale"), locale);
}

uint16_t HostSettings::tcpPort() const
{
    return settings_.value(QLatin1String("TcpPort"), DEFAULT_HOST_TCP_PORT).toUInt();
}

void HostSettings::setTcpPort(uint16_t port)
{
    settings_.setValue(QLatin1String("TcpPort"), port);
}

bool HostSettings::addFirewallRule() const
{
    return settings_.value(QLatin1String("AddFirewallRule"), true).toBool();
}

void HostSettings::setAddFirewallRule(bool value)
{
    settings_.setValue(QLatin1String("AddFirewallRule"), value);
}

SrpUserList HostSettings::userList() const
{
    SrpUserList users;

    users.seed_key = settings_.value(QLatin1String("SeedKey")).toByteArray();
    if (users.seed_key.isEmpty())
        users.seed_key = Random::generateBuffer(64);

    int size = settings_.beginReadArray(QLatin1String("Users"));
    for (int i = 0; i < size; ++i)
    {
        settings_.setArrayIndex(i);

        SrpUser user;
        user.name      = settings_.value(QLatin1String("Name")).toString();
        user.salt      = QByteArray::fromBase64(settings_.value(QLatin1String("Salt")).toByteArray());
        user.verifier  = QByteArray::fromBase64(settings_.value(QLatin1String("Verifier")).toByteArray());
        user.number    = QByteArray::fromBase64(settings_.value(QLatin1String("Number")).toByteArray());
        user.generator = QByteArray::fromBase64(settings_.value(QLatin1String("Generator")).toByteArray());
        user.sessions  = settings_.value(QLatin1String("Sessions")).toUInt();
        user.flags     = settings_.value(QLatin1String("Flags")).toUInt();

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

void HostSettings::setUserList(const SrpUserList& users)
{
    // Clear the old list of users.
    settings_.remove(QLatin1String("Users"));

    settings_.setValue(QLatin1String("SeedKey"), users.seed_key.toBase64());

    settings_.beginWriteArray(QLatin1String("Users"));
    for (int i = 0; i < users.list.size(); ++i)
    {
        settings_.setArrayIndex(i);

        const SrpUser& user = users.list.at(i);

        settings_.setValue(QLatin1String("Name"), user.name);
        settings_.setValue(QLatin1String("Salt"), user.salt.toBase64());
        settings_.setValue(QLatin1String("Verifier"), user.verifier.toBase64());
        settings_.setValue(QLatin1String("Number"), user.number.toBase64());
        settings_.setValue(QLatin1String("Generator"), user.generator.toBase64());
        settings_.setValue(QLatin1String("Sessions"), user.sessions);
        settings_.setValue(QLatin1String("Flags"), user.flags);
    }
    settings_.endArray();
}

QString HostSettings::updateServer() const
{
    return settings_.value(QLatin1String("UpdateServer"), DEFAULT_UPDATE_SERVER).toString();
}

void HostSettings::setUpdateServer(const QString& server)
{
    settings_.setValue(QLatin1String("UpdateServer"), server);
}

bool HostSettings::remoteUpdate() const
{
    return settings_.value(QLatin1String("RemoteUpdate"), true).toBool();
}

void HostSettings::setRemoteUpdate(bool allow)
{
    settings_.setValue(QLatin1String("RemoteUpdate"), allow);
}

} // namespace aspia
