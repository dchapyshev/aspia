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

#ifndef CLIENT_STORAGE_USER_SETTINGS_H
#define CLIENT_STORAGE_USER_SETTINGS_H

#include <QSettings>

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

// Settings of the user that are common to all databases.
class UserSettings
{
public:
    UserSettings();
    ~UserSettings();

    QString locale() const;
    void setLocale(const QString& locale);

    QString theme() const;
    void setTheme(const QString& theme);

    QString lastDatabase() const;
    void setLastDatabase(const QString& id);

    proto::peer::SessionType sessionType() const;
    void setSessionType(proto::peer::SessionType session_type);

    bool isOneTimePasswordChecked() const;
    void setOneTimePasswordChecked(bool check);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);

    QVariantMap quickConnectState() const;
    void setQuickConnectState(const QVariantMap& state);

    QByteArray dialogGeometry(const QString& name) const;
    void setDialogGeometry(const QString& name, const QByteArray& geometry);

    bool largeIcons() const;
    void setLargeIcons(bool enable);

    bool isToolBarEnabled() const;
    void setToolBarEnabled(bool enable);

    bool isSearchFieldEnabled() const;
    void setSearchFieldEnabled(bool enable);

    bool alwaysOnTop() const;
    void setAlwaysOnTop(bool enable);

    bool openSessionsInTabs() const;
    void setOpenSessionsInTabs(bool enable);

    bool isOnlineCheckEnabled() const;
    void setOnlineCheckEnabled(bool enable);

    QString lastDirectory() const;
    void setLastDirectory(const QString& path);

    bool isConfirmCloseEnabled() const;
    void setConfirmCloseEnabled(bool enable);

    bool isCheckUpdatesEnabled() const;
    void setCheckUpdatesEnabled(bool enable);

    QString updateChannel() const;
    void setUpdateChannel(const QString& channel);

    bool isMcpEnabled() const;
    void setMcpEnabled(bool enable);

    bool isMcpAutoConfirmEnabled() const;
    void setMcpAutoConfirmEnabled(bool enable);

    bool isMcpSignInEnabled() const;
    void setMcpSignInEnabled(bool enable);

    quint32 mcpSessionTypes() const;
    void setMcpSessionTypes(quint32 session_types);

private:
    QSettings settings_;
    Q_DISABLE_COPY_MOVE(UserSettings)
};

#endif // CLIENT_STORAGE_USER_SETTINGS_H
