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

#ifndef CLIENT_UI_CLIENT_SETTINGS_H
#define CLIENT_UI_CLIENT_SETTINGS_H

#include "base/macros_magic.h"
#include "proto/desktop.h"
#include "proto/peer.h"

#include <QSettings>

namespace client {

class ClientSettings
{
public:
    ClientSettings();
    ~ClientSettings() = default;

    QString locale() const;
    void setLocale(const QString& locale);

    QStringList addressList() const;
    void setAddressList(const QStringList& list);

    proto::peer::SessionType sessionType() const;
    void setSessionType(proto::peer::SessionType session_type);

    proto::desktop::Config desktopManageConfig() const;
    void setDesktopManageConfig(const proto::desktop::Config& config);

    proto::desktop::Config desktopViewConfig() const;
    void setDesktopViewConfig(const proto::desktop::Config& config);

    bool showIconsInMenus() const;
    void setShowIconsInMenus(bool enable);

    bool checkUpdates() const;
    void setCheckUpdates(bool check);

    QString updateServer() const;
    void setUpdateServer(const QString& server);

    bool isOneTimePasswordChecked() const;
    void setOneTimePasswordChecked(bool check);

    QByteArray routerManagerState() const;
    void setRouterManagerState(const QByteArray& state);

    QString displayName() const;
    void setDisplayName(const QString& display_name);

private:
    QSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(ClientSettings);
};

} // namespace client

#endif // CLIENT_UI_CLIENT_SETTINGS_H
