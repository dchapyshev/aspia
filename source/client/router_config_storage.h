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

#ifndef CLIENT__ROUTER_CONFIG_STORAGE_H
#define CLIENT__ROUTER_CONFIG_STORAGE_H

#include "base/macros_magic.h"
#include "base/settings/json_settings.h"
#include "client/router_config.h"

namespace client {

class RouterConfigStorage
{
public:
    ~RouterConfigStorage();

    static std::unique_ptr<RouterConfigStorage> open();

    RouterConfigList routerConfigList();
    void setRouterConfigList(const RouterConfigList& configs);

private:
    RouterConfigStorage();

    base::JsonSettings storage_;

    DISALLOW_COPY_AND_ASSIGN(RouterConfigStorage);
};

} // namespace client

#endif // CLIENT__ROUTER_CONFIG_STORAGE_H
