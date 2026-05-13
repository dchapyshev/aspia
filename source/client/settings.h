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

namespace proto::control {
class Config;
} // namespace proto::control

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

// Stores non-critical UI/preference settings.
//
// On application upgrade (when the stored version is older than the current one), all parameters
// are cleared on first construction in the process. Therefore this class must NOT be used for
// connection-related, security-related or any other critical data: such values should be stored
// in the Database (see client/database.h) or another persistent storage that survives upgrades.
class Settings
{
public:
    Settings();
    ~Settings();

    QString locale() const;
    void setLocale(const QString& locale);

    QString theme() const;
    void setTheme(const QString& theme);

    proto::peer::SessionType sessionType() const;
    void setSessionType(proto::peer::SessionType session_type);

    proto::control::Config desktopConfig() const;
    void setDesktopConfig(const proto::control::Config& config);

    bool isOneTimePasswordChecked() const;
    void setOneTimePasswordChecked(bool check);

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

    bool isOnlineCheckEnabled() const;
    void setOnlineCheckEnabled(bool enable);

    bool alwaysOnTop() const;
    void setAlwaysOnTop(bool enable);

    bool openSessionsInTabs() const;
    void setOpenSessionsInTabs(bool enable);

    QString recordingPath() const;
    void setRecordingPath(const QString& path);

    bool recordSessions() const;
    void setRecordSessions(bool enable);

    bool sendKeyCombinations() const;
    void setSendKeyCombinations(bool enable);

    bool isUdpAllowed() const;
    void setUdpAllowed(bool enable);

    QByteArray tabState(const QString& name) const;
    void setTabState(const QString& name, const QByteArray& state);

    bool isGroupExpanded(qint64 group_id) const;
    void setGroupExpanded(qint64 group_id, bool expanded);
    void removeGroupExpanded(qint64 group_id);

private:
    QSettings settings_;
    Q_DISABLE_COPY_MOVE(Settings)
};

#endif // CLIENT_SETTINGS_H
