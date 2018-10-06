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

#include <filesystem>
#include <fstream>

#include <boost/property_tree/xml_parser.hpp>

#include "base/base_paths.h"
#include "base/base64.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "crypto/random.h"
#include "network/srp_user.h"

namespace aspia {

namespace {

const char kDirName[] = "aspia";
const char kFileName[] = "host.xml";

bool settingsFilePath(std::filesystem::path* path, bool create_dir = false)
{
    std::filesystem::path temp;
    if (!BasePaths::commonAppData(&temp))
        return false;

    temp.append(kDirName);

    if (create_dir)
    {
        std::error_code ignored_code;
        if (!std::filesystem::exists(temp, ignored_code))
        {
            if (!std::filesystem::create_directories(temp, ignored_code))
            {
                LOG(LS_WARNING) << "Unable to create directory: " << temp;
                return false;
            }
        }
    }

    temp.append(kFileName);

    path->swap(temp);
    return true;
}

bool readSettingsFile(const std::filesystem::path& path, boost::property_tree::ptree* tree)
{
    std::ifstream file;
    file.open(path, std::ifstream::binary);
    if (!file.is_open())
    {
        LOG(LS_WARNING) << "Unable to open file " << path << " for reading";
        return false;
    }

    try
    {
        boost::property_tree::read_xml(file, *tree);
    }
    catch (const boost::property_tree::ptree_error &err)
    {
        LOG(LS_WARNING) << "Error reading the configuration: " << err.what();
        return false;
    }

    return true;
}

bool writeSettingsFile(const std::filesystem::path& path, const boost::property_tree::ptree& tree)
{
    std::ofstream file;
    file.open(path, std::ofstream::binary);
    if (!file.is_open())
    {
        LOG(LS_WARNING) << "Unable to open file " << path << " for write";
        return false;
    }

    try
    {
        boost::property_tree::write_xml(file, tree);
    }
    catch (const boost::property_tree::ptree_error &err)
    {
        LOG(LS_WARNING) << "Error writing the configuration: " << err.what();
        return false;
    }

    return true;
}

} // namespace

HostSettings::HostSettings()
{
    std::filesystem::path path;
    if (settingsFilePath(&path))
        readSettingsFile(path, &tree_);
}

HostSettings::~HostSettings() = default;

// static
HostSettings::ImportResult HostSettings::importSettings(const std::string& from)
{
    boost::property_tree::ptree tree;
    if (!readSettingsFile(std::filesystem::u8path(from), &tree))
        return ImportResult::READ_ERROR;

    std::filesystem::path to;
    if (!settingsFilePath(&to, true))
        return ImportResult::WRITE_ERROR;

    if (!writeSettingsFile(to, tree))
        return ImportResult::WRITE_ERROR;

    return ImportResult::SUCCESS;
}

// static
HostSettings::ExportResult HostSettings::exportSettings(const std::string& to)
{
    std::filesystem::path from;
    if (!settingsFilePath(&from))
        return ExportResult::READ_ERROR;

    boost::property_tree::ptree tree;
    if (!readSettingsFile(from, &tree))
        return ExportResult::READ_ERROR;

    if (!writeSettingsFile(std::filesystem::u8path(to), tree))
        return ExportResult::WRITE_ERROR;

    return ExportResult::SUCCESS;
}

bool HostSettings::commit()
{
    std::filesystem::path path;
    if (!settingsFilePath(&path, true))
        return false;

    return writeSettingsFile(path, tree_);
}

std::string HostSettings::locale() const
{
    return tree_.get<std::string>("locale", "en");
}

void HostSettings::setLocale(const std::string& locale)
{
    tree_.put("locale", locale);
}

uint16_t HostSettings::tcpPort() const
{
    return tree_.get<uint16_t>("tcp_port", DEFAULT_HOST_TCP_PORT);
}

void HostSettings::setTcpPort(uint16_t port)
{
    tree_.put("tcp_port", port);
}

bool HostSettings::addFirewallRule() const
{
    return tree_.get<bool>("add_firewall_rule", true);
}

void HostSettings::setAddFirewallRule(bool value)
{
    tree_.put("add_firewall_rule", value);
}

SrpUserList HostSettings::userList() const
{
    SrpUserList users;

    try
    {
        boost::property_tree::ptree empty_tree;

        for (const auto& child_node : tree_.get_child("srp_users", empty_tree))
        {
            boost::property_tree::ptree user_tree = child_node.second;

            SrpUser user;
            user.name      = user_tree.get<std::string>("name");
            user.salt      = user_tree.get<std::string>("salt");
            user.verifier  = user_tree.get<std::string>("verifier");
            user.number    = user_tree.get<std::string>("number");
            user.generator = user_tree.get<std::string>("generator");
            user.sessions  = user_tree.get<uint32_t>("sessions");
            user.flags     = user_tree.get<uint32_t>("flags");

            CHECK(!user.name.empty());
            CHECK(Base64::decode(user.salt, &user.salt));
            CHECK(Base64::decode(user.verifier, &user.verifier));
            CHECK(Base64::decode(user.number, &user.number));
            CHECK(Base64::decode(user.generator, &user.generator));

            users.list.push_back(std::move(user));
        }

        users.seed_key = tree_.get<std::string>("srp_seed_key", "");
        Base64::decode(users.seed_key, &users.seed_key);
    }
    catch (const boost::property_tree::ptree_error &err)
    {
        LOG(LS_WARNING) << "Error reading the list of users: " << err.what();
    }

    if (users.seed_key.empty())
        users.seed_key = Random::generateBuffer(64);

    return users;
}

void HostSettings::setUserList(const SrpUserList& users)
{
    // Clear the old list of users.
    tree_.erase("srp_users");

    for (const auto& user : users.list)
    {
        boost::property_tree::ptree user_tree;

        user_tree.put<std::string>("name", user.name);
        user_tree.put<std::string>("salt", Base64::encode(user.salt));
        user_tree.put<std::string>("verifier", Base64::encode(user.verifier));
        user_tree.put<std::string>("number", Base64::encode(user.number));
        user_tree.put<std::string>("generator", Base64::encode(user.generator));
        user_tree.put<uint32_t>("sessions", user.sessions);
        user_tree.put<uint32_t>("flags", user.flags);

        tree_.add_child("srp_users.user", user_tree);
    }

    tree_.put<std::string>("srp_seed_key", Base64::encode(users.seed_key));
}

std::string HostSettings::updateServer() const
{
    return tree_.get<std::string>("update_server", DEFAULT_UPDATE_SERVER);
}

void HostSettings::setUpdateServer(const std::string& server)
{
    tree_.put("update_server", server);
}

bool HostSettings::customUpdateServer() const
{
    return tree_.get<bool>("custom_update_server", false);
}

void HostSettings::setCustomUpdateServer(bool use)
{
    tree_.put("custom_update_server", use);
}

bool HostSettings::remoteUpdate() const
{
    return tree_.get<bool>("remote_update", true);
}

void HostSettings::setRemoteUpdate(bool allow)
{
    tree_.put("remote_update", allow);
}

} // namespace aspia
