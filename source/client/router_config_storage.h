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

#ifndef CLIENT_ROUTER_CONFIG_STORAGE_H
#define CLIENT_ROUTER_CONFIG_STORAGE_H

#include <QSettings>

#include "client/router_config.h"

namespace client {

class RouterConfigStorage
{
public:
    RouterConfigStorage();
    ~RouterConfigStorage();

    bool isEnabled() const;
    void setEnabled(bool enable);

    RouterConfig routerConfig() const;
    void setRouterConfig(const RouterConfig& config);

private:
    QSettings storage_;
    Q_DISABLE_COPY(RouterConfigStorage)
};

} // namespace client

#endif // CLIENT_ROUTER_CONFIG_STORAGE_H
