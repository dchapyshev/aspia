//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/system_settings.h"

#include "crypto/random.h"
#include "host/user.h"

namespace host {

SystemSettings::SystemSettings()
    : settings_(base::XmlSettings::Scope::SYSTEM, "aspia", "host")
{
    // Nothing
}

SystemSettings::~SystemSettings() = default;

const std::filesystem::path& SystemSettings::filePath() const
{
    return settings_.filePath();
}

bool SystemSettings::isWritable() const
{
    return settings_.isWritable();
}

void SystemSettings::sync()
{
    settings_.sync();
}

uint16_t SystemSettings::tcpPort() const
{
    return settings_.get<uint16_t>("TcpPort", DEFAULT_HOST_TCP_PORT);
}

void SystemSettings::setTcpPort(uint16_t port)
{
    settings_.set<uint16_t>("TcpPort", port);
}

UserList SystemSettings::userList() const
{
    UserList users;

    for (const auto& item : settings_.getArray("Users"))
    {
        User user;

        user.name = item.get<std::u16string>("Name");
        user.salt = item.get<base::ByteArray>("Salt");
        user.verifier = item.get<base::ByteArray>("Verifier");
        user.number = item.get<base::ByteArray>("Number");
        user.generator = item.get<base::ByteArray>("Generator");
        user.sessions = item.get<uint32_t>("Sessions");
        user.flags = item.get<uint32_t>("Flags");

        users.add(std::move(user));
    }

    base::ByteArray seed_key = settings_.get<base::ByteArray>("SeedKey");
    if (seed_key.empty())
        seed_key = crypto::Random::byteArray(64);

    users.setSeedKey(seed_key);

    return users;
}

void SystemSettings::setUserList(const UserList& users)
{
    // Clear the old list of users.
    settings_.remove("Users");

    base::Settings::Array users_array;

    for (UserList::Iterator it(users); !it.isAtEnd(); it.advance())
    {
        const User& user = it.user();

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

    settings_.setArray("Users", users_array);
    settings_.set("SeedKey", users.seedKey());
}

std::string SystemSettings::updateServer() const
{
    return settings_.get<std::string>("UpdateServer", DEFAULT_UPDATE_SERVER);
}

void SystemSettings::setUpdateServer(const std::string& server)
{
    settings_.set("UpdateServer", server);
}

} // namespace host
