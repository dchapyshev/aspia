//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_SETTINGS_H
#define CLIENT_SETTINGS_H

#include <QSettings>

#include "client/config.h"
#include "proto/desktop_control.h"
#include "proto/peer.h"

namespace client {

class Settings
{
public:
    Settings();
    ~Settings() = default;

    QString locale() const;
    void setLocale(const QString& locale);

    QString theme() const;
    void setTheme(const QString& theme);

    proto::peer::SessionType sessionType() const;
    void setSessionType(proto::peer::SessionType session_type);

    proto::control::Config desktopConfig() const;
    void setDesktopConfig(const proto::control::Config& config);

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

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);

    bool largeIcons() const;
    void setLargeIcons(bool enable);

    bool isToolBarEnabled() const;
    void setToolBarEnabled(bool enable);

    bool isStatusBarEnabled() const;
    void setStatusBarEnabled(bool enable);

    QByteArray tabState(const QString& name) const;
    void setTabState(const QString& name, const QByteArray& state);

private:
    QSettings settings_;
    Q_DISABLE_COPY_MOVE(Settings)
};

} // namespace client

#endif // CLIENT_SETTINGS_H
