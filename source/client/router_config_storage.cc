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
#include "base/xml_settings.h"

namespace client {

//--------------------------------------------------------------------------------------------------
RouterConfigStorage::RouterConfigStorage()
    : storage_(base::XmlSettings::format(), QSettings::UserScope, "aspia", "router_config")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
RouterConfigStorage::~RouterConfigStorage() = default;

//--------------------------------------------------------------------------------------------------
bool RouterConfigStorage::isEnabled() const
{
    return storage_.value("enabled", false).toBool();
}

//--------------------------------------------------------------------------------------------------
void RouterConfigStorage::setEnabled(bool enable)
{
    storage_.setValue("enabled", enable);
}

//--------------------------------------------------------------------------------------------------
RouterConfig RouterConfigStorage::routerConfig() const
{
    RouterConfig config;

    config.address = storage_.value("address").toString();
    config.port = storage_.value("port").toUInt();
    config.username = storage_.value("username").toString();
    config.password = storage_.value("password").toString();

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

    storage_.setValue("address", config.address);
    storage_.setValue("port", config.port);
    storage_.setValue("username", config.username);
    storage_.setValue("password", config.password);
}

} // namespace client
