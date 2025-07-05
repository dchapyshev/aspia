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

#ifndef CONSOLE_SETTINGS_H
#define CONSOLE_SETTINGS_H

#include <QSettings>

#include "proto/peer.h"

namespace console {

class Settings
{
public:
    Settings();
    ~Settings() = default;

    QString locale() const;
    void setLocale(const QString& locale);

    QString lastDirectory() const;
    void setLastDirectory(const QString& directory_path);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);

    QByteArray addressBookState() const;
    void setAddressBookState(const QByteArray& state);

    bool isRecentOpenEnabled() const;
    void setRecentOpenEnabled(bool enable);

    QStringList recentOpen() const;
    void setRecentOpen(const QStringList& list);

    QStringList pinnedFiles() const;
    void setPinnedFiles(const QStringList& list);

    bool isToolBarEnabled() const;
    void setToolBarEnabled(bool enable);

    bool isStatusBarEnabled() const;
    void setStatusBarEnabled(bool enable);

    bool minimizeToTray() const;
    void setMinimizeToTray(bool enable);

    bool alwaysShowTrayIcon() const;
    void setAlwaysShowTrayIcon(bool enable);

    proto::peer::SessionType sessionType() const;
    void setSessionType(proto::peer::SessionType session_type);

    bool checkUpdates() const;
    void setCheckUpdates(bool check);

    QString updateServer() const;
    void setUpdateServer(const QString& server);

    QByteArray computerDialogGeometry() const;
    void setComputerDialogGeometry(const QByteArray& geometry);

    QByteArray computerDialogState() const;
    void setComputerDialogState(const QByteArray& state);

    QByteArray computerGroupDialogGeometry() const;
    void setComputerGroupDialogGeometry(const QByteArray& geometry);

    QByteArray fastConnectConfig(const QString& guid);
    void setFastConnectConfig(const QString& guid, const QByteArray& config);

    bool showIconsInMenus() const;
    void setShowIconsInMenus(bool enable);

    QByteArray addressBookDialogGeometry() const;
    void setAddressBookDialogGeometry(const QByteArray& geometry);

    bool largeIcons() const;
    void setLargeIcons(bool enable);

private:
    QSettings settings_;
    Q_DISABLE_COPY(Settings)
};

} // namespace console

#endif // CONSOLE_SETTINGS_H
