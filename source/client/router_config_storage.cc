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
#include "base/crypto/os_crypt.h"

namespace client {

RouterConfigStorage::RouterConfigStorage()
    : storage_(base::JsonSettings::Scope::USER, "aspia", "router_storage")
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

        std::string cipher_password = item.get<std::string>("password");
        if (cipher_password.empty())
        {
            LOG(LS_ERROR) << "Invalid 'password' field";
            return RouterConfigList();
        }

        if (!base::OSCrypt::decryptString16(cipher_password, &config.password))
        {
            LOG(LS_ERROR) << "Invalid 'password' field";
            return RouterConfigList();
        }

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

        std::string cipher_password;
        if (!base::OSCrypt::encryptString16(config.password, &cipher_password))
        {
            LOG(LS_ERROR) << "Invalid 'password' filed";
            return;
        }

        item.set("name", config.name);
        item.set("address", config.address);
        item.set("port", config.port);
        item.set("username", config.username);
        item.set("password", cipher_password);

        array.emplace_back(std::move(item));
    }

    storage_.remove("routers");
    storage_.setArray("routers", array);
}

} // namespace client
