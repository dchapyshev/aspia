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

void readSettingsFile(boost::property_tree::ptree* tree)
{
    std::filesystem::path path;
    if (!BasePaths::commonAppData(&path))
        return;

    path.append(kDirName);
    path.append(kFileName);

    std::ifstream file;
    file.open(path, std::ifstream::binary);
    if (!file.is_open())
        return;

    boost::property_tree::read_xml(file, *tree);
}

bool writeSettingsFile(const boost::property_tree::ptree& tree)
{
    std::filesystem::path path;
    if (!BasePaths::commonAppData(&path))
        return false;

    path.append(kDirName);

    std::error_code ignored_code;
    if (!std::filesystem::exists(path, ignored_code))
    {
        if (!std::filesystem::create_directories(path, ignored_code))
            return false;
    }

    path.append(kFileName);

    std::ofstream file;
    file.open(path, std::ofstream::binary);
    if (!file.is_open())
        return false;

    boost::property_tree::write_xml(
        file, tree, boost::property_tree::xml_writer_make_settings<std::string>(' ', 2));
    return true;
}

} // namespace

HostSettings::HostSettings()
{
    readSettingsFile(&tree_);
}

HostSettings::~HostSettings() = default;

bool HostSettings::commit()
{
    return writeSettingsFile(tree_);
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

std::shared_ptr<SrpUserList> HostSettings::userList() const
{
    try
    {
        std::shared_ptr<SrpUserList> users = std::make_shared<SrpUserList>();

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

            users->list.push_back(std::move(user));
        }

        users->seed_key = tree_.get<std::string>("srp_seed_key", "");

        Base64::decode(users->seed_key, &users->seed_key);
        if (users->seed_key.empty())
            users->seed_key = Random::generateBuffer(512);

        return users;
    }
    catch (const boost::property_tree::ptree_error &err)
    {
        LOG(LS_WARNING) << "Error reading the list of users: " << err.what();
    }

    return nullptr;
}

void HostSettings::setUserList(const SrpUserList& users)
{
    // Clear the old list of users.
    tree_.erase("srp_users");

    std::string buffer;

    for (const auto& user : users.list)
    {
        boost::property_tree::ptree user_tree;

        user_tree.put<std::string>("name", user.name);
        user_tree.put<uint32_t>("sessions", user.sessions);
        user_tree.put<uint32_t>("flags", user.flags);

        Base64::encode(user.salt, &buffer);
        user_tree.put<std::string>("salt", buffer);

        Base64::encode(user.verifier, &buffer);
        user_tree.put<std::string>("verifier", buffer);

        Base64::encode(user.number, &buffer);
        user_tree.put<std::string>("number", buffer);

        Base64::encode(user.generator, &buffer);
        user_tree.put<std::string>("generator", buffer);

        tree_.add_child("srp_users.user", user_tree);
    }

    Base64::encode(users.seed_key, &buffer);
    tree_.put<std::string>("srp_seed_key", buffer);
}

} // namespace aspia
