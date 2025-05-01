//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

//--------------------------------------------------------------------------------------------------
RouterConfigStorage::RouterConfigStorage()
    : storage_(base::JsonSettings::Scope::USER,
               "aspia",
               "router_config",
               base::JsonSettings::Encrypted::YES)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
RouterConfigStorage::~RouterConfigStorage() = default;

//--------------------------------------------------------------------------------------------------
bool RouterConfigStorage::isEnabled() const
{
    return storage_.get<bool>("enabled", false);
}

//--------------------------------------------------------------------------------------------------
void RouterConfigStorage::setEnabled(bool enable)
{
    storage_.set("enabled", enable);
}

//--------------------------------------------------------------------------------------------------
RouterConfig RouterConfigStorage::routerConfig() const
{
    RouterConfig config;

    config.address = storage_.get<QString>("address");
    config.port = storage_.get<quint16>("port");
    config.username = storage_.get<QString>("username");
    config.password = storage_.get<QString>("password");

    return config;
}

//--------------------------------------------------------------------------------------------------
void RouterConfigStorage::setRouterConfig(const RouterConfig& config)
{
    if (!config.isValid())
    {
        LOG(LS_ERROR) << "Invalid router config";
        return;
    }

    storage_.set("address", config.address);
    storage_.set("port", config.port);
    storage_.set("username", config.username);
    storage_.set("password", config.password);
}

} // namespace client
