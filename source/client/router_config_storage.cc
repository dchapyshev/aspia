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

#include "client/router_config_storage.h"

#include "base/logging.h"

namespace client {

RouterConfigStorage::RouterConfigStorage()
    : storage_(base::JsonSettings::Scope::USER,
               "aspia",
               "routers",
               base::JsonSettings::Encrypted::YES)
{
    // Nothing
}

RouterConfigStorage::~RouterConfigStorage() = default;

// static
std::unique_ptr<RouterConfigStorage> RouterConfigStorage::open()
{
    return std::unique_ptr<RouterConfigStorage>(new RouterConfigStorage());
}

RouterConfigList RouterConfigStorage::routerConfigList()
{
    RouterConfigList list;

    for (const auto& item : storage_.getArray("routers"))
    {
        RouterConfig config;

        config.guid = item.get<std::string>("guid");
        if (config.guid.empty())
        {
            LOG(LS_ERROR) << "Invalid 'guid' field";
            return RouterConfigList();
        }

        config.name = item.get<std::u16string>("name");
        if (config.name.empty())
        {
            LOG(LS_ERROR) << "Invalid 'name' field";
            return RouterConfigList();
        }

        config.address = item.get<std::u16string>("address");
        if (config.address.empty())
        {
            LOG(LS_ERROR) << "Invalid 'address' filed";
            return RouterConfigList();
        }

        config.port = item.get<uint16_t>("port");
        if (!config.port)
        {
            LOG(LS_ERROR) << "Invalid 'port' field";
            return RouterConfigList();
        }

        config.username = item.get<std::u16string>("username");
        if (config.username.empty())
        {
            LOG(LS_ERROR) << "Invalid 'username' field";
            return RouterConfigList();
        }

        config.password = item.get<std::u16string>("password");
        if (config.password.empty())
        {
            LOG(LS_ERROR) << "Invalid 'password' field";
            return RouterConfigList();
        }

        config.comment = item.get<std::u16string>("comment");

        list.emplace_back(std::move(config));
    }

    return list;
}

void RouterConfigStorage::setRouterConfigList(const RouterConfigList& configs)
{
    base::Settings::Array array;

    for (const auto& config : configs)
    {
        base::Settings item;

        if (config.guid.empty())
        {
            LOG(LS_ERROR) << "Invalid 'guid' field";
            return;
        }

        if (config.name.empty())
        {
            LOG(LS_ERROR) << "Invalid 'name' field";
            return;
        }

        if (config.address.empty())
        {
            LOG(LS_ERROR) << "Invalid 'address' field";
            return;
        }

        if (!config.port)
        {
            LOG(LS_ERROR) << "Invalid 'port' field";
            return;
        }

        if (config.username.empty())
        {
            LOG(LS_ERROR) << "Invalid 'username' field";
            return;
        }

        if (config.password.empty())
        {
            LOG(LS_ERROR) << "Invalid 'password' field";
            return;
        }

        item.set("guid", config.guid);
        item.set("name", config.name);
        item.set("address", config.address);
        item.set("port", config.port);
        item.set("username", config.username);
        item.set("password", config.password);
        item.set("comment", config.comment);

        array.emplace_back(std::move(item));
    }

    storage_.remove("routers");
    storage_.setArray("routers", array);
}

} // namespace client
