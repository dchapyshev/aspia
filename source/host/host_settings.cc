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

#include "base/message_serialization.h"
#include "crypto/random.h"

namespace aspia {

HostSettings::HostSettings()
    : settings_(QSettings::SystemScope, QStringLiteral("Aspia"), QStringLiteral("Host"))
{
    // Nothing
}

bool HostSettings::isWritable() const
{
    return settings_.isWritable();
}

// static
QString HostSettings::defaultLocale()
{
    return QStringLiteral("en");
}

QString HostSettings::locale() const
{
    return settings_.value(QStringLiteral("Locale"), defaultLocale()).toString();
}

void HostSettings::setLocale(const QString& locale)
{
    settings_.setValue(QStringLiteral("Locale"), locale);
}

int HostSettings::tcpPort() const
{
    return settings_.value(QStringLiteral("TcpPort"), kDefaultHostTcpPort).toInt();
}

bool HostSettings::setTcpPort(int port)
{
    if (!settings_.isWritable())
        return false;

    settings_.setValue(QStringLiteral("TcpPort"), port);
    return true;
}

std::shared_ptr<proto::SrpUserList> HostSettings::userList() const
{
    QByteArray serialized_user_list = settings_.value(QStringLiteral("SrpUserList")).toByteArray();

    std::shared_ptr<proto::SrpUserList> user_list = std::make_shared<proto::SrpUserList>();

    if (!parseMessage(serialized_user_list, *user_list))
    {
        LOG(LS_WARNING) << "The list of users is corrupted";
        return nullptr;
    }

    if (user_list->seed_key().empty())
    {
        static const size_t kSeedKeySize = 512;
        user_list->set_seed_key(Random::generateBuffer(kSeedKeySize));
    }

    return user_list;
}

bool HostSettings::setUserList(const proto::SrpUserList& user_list)
{
    if (!settings_.isWritable())
        return false;

    if (user_list.seed_key().empty())
        return false;

    QByteArray serialized_user_list = serializeMessage(user_list);

    settings_.setValue(QStringLiteral("SrpUserList"), serialized_user_list);
    return true;
}

} // namespace aspia
