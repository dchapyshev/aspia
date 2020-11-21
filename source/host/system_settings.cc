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

#include "base/base64.h"
#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/peer/user_list.h"
#include "base/settings/xml_settings.h"

namespace host {

namespace {

// This should be removed in the next release.
void settingsMigration(base::JsonSettings* json_settings)
{
    std::filesystem::path old_file = base::XmlSettings::filePath(
        base::XmlSettings::Scope::SYSTEM, "aspia", "host");

    std::error_code ignored_error;
    if (!std::filesystem::exists(old_file, ignored_error))
    {
        // There is no old parameter file, exit.
        return;
    }

    base::XmlSettings xml_settings(base::XmlSettings::Scope::SYSTEM, "aspia", "host");

    // TcpPort
    json_settings->set<uint16_t>(
        "TcpPort", xml_settings.get<uint16_t>("TcpPort", DEFAULT_HOST_TCP_PORT));

    // AddFirewallRules
    json_settings->set<bool>(
        "AddFirewallRules", xml_settings.get<bool>("AddFirewallRules", true));

    // UpdateServer
    json_settings->set<std::u16string>(
        "UpdateServer", xml_settings.get<std::u16string>("UpdateServer", DEFAULT_UPDATE_SERVER));

    // Users
    base::Settings::Array users_array;
    for (const auto& old_item : xml_settings.getArray("Users"))
    {
        base::Settings new_item;
        new_item.set<std::string>("Name", old_item.get<std::string>("Name"));
        new_item.set<std::string>("Group", "8192");

        new_item.set<base::ByteArray>("Salt",
            base::Base64::decodeT<std::string, base::ByteArray>(
                old_item.get<std::string>("Salt")));

        new_item.set<base::ByteArray>("Verifier",
            base::Base64::decodeT<std::string, base::ByteArray>(
                old_item.get<std::string>("Verifier")));

        new_item.set<uint32_t>("Sessions", old_item.get<uint32_t>("Sessions"));
        new_item.set<uint32_t>("Flags", old_item.get<uint32_t>("Flags"));
        users_array.emplace_back(std::move(new_item));
    }

    json_settings->remove("Users");
    json_settings->setArray("Users", users_array);
    json_settings->set("SeedKey",
        base::Base64::decodeT<std::string, base::ByteArray>(
            xml_settings.get<std::string>("SeedKey")));

    if (!std::filesystem::remove(old_file, ignored_error))
    {
        LOG(LS_WARNING) << "Failed to delete old settings file";
    }
}

} // namespace

SystemSettings::SystemSettings()
    : settings_(base::JsonSettings::Scope::SYSTEM, "aspia", "host")
{
    settingsMigration(&settings_);
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

bool SystemSettings::isRouterEnabled() const
{
    return settings_.get<bool>("RouterEnabled", false);
}

void SystemSettings::setRouterEnabled(bool enable)
{
    settings_.set<bool>("RouterEnabled", enable);
}

std::u16string SystemSettings::routerAddress() const
{
    return settings_.get<std::u16string>("RouterAddress");
}

void SystemSettings::setRouterAddress(const std::u16string& address)
{
    settings_.set<std::u16string>("RouterAddress", address);
}

uint16_t SystemSettings::routerPort() const
{
    return settings_.get<uint16_t>("RouterPort", DEFAULT_ROUTER_TCP_PORT);
}

void SystemSettings::setRouterPort(uint16_t port)
{
    settings_.set<uint16_t>("RouterPort", port);
}

base::ByteArray SystemSettings::routerPublicKey() const
{
    return settings_.get<base::ByteArray>("RouterPublicKey");
}

void SystemSettings::setRouterPublicKey(const base::ByteArray& key)
{
    settings_.set<base::ByteArray>("RouterPublicKey", key);
}

std::unique_ptr<base::UserList> SystemSettings::userList() const
{
    std::unique_ptr<base::UserList> users = base::UserList::createEmpty();

    for (const auto& item : settings_.getArray("Users"))
    {
        base::User user;

        user.name     = item.get<std::u16string>("Name");
        user.group    = item.get<std::string>("Group");
        user.salt     = item.get<base::ByteArray>("Salt");
        user.verifier = item.get<base::ByteArray>("Verifier");
        user.sessions = item.get<uint32_t>("Sessions");
        user.flags    = item.get<uint32_t>("Flags");

        users->add(user);
    }

    base::ByteArray seed_key = settings_.get<base::ByteArray>("SeedKey");
    if (seed_key.empty())
        seed_key = base::Random::byteArray(64);

    users->setSeedKey(seed_key);
    return users;
}

void SystemSettings::setUserList(const base::UserList& users)
{
    // Clear the old list of users.
    settings_.remove("Users");

    base::Settings::Array users_array;

    for (const auto& user : users.list())
    {
        base::Settings item;
        item.set("Name", user.name);
        item.set("Group", user.group);
        item.set("Salt", user.salt);
        item.set("Verifier", user.verifier);
        item.set("Sessions", user.sessions);
        item.set("Flags", user.flags);

        users_array.emplace_back(std::move(item));
    }

    settings_.setArray("Users", users_array);
    settings_.set("SeedKey", users.seedKey());
}

std::u16string SystemSettings::updateServer() const
{
    return settings_.get<std::u16string>("UpdateServer", DEFAULT_UPDATE_SERVER);
}

void SystemSettings::setUpdateServer(const std::u16string& server)
{
    settings_.set("UpdateServer", server);
}

uint32_t SystemSettings::preferredVideoCapturer() const
{
    return settings_.get<uint32_t>("PreferredVideoCapturer", 0);
}

void SystemSettings::setPreferredVideoCapturer(uint32_t type)
{
    settings_.set("PreferredVideoCapturer", type);
}

} // namespace host
