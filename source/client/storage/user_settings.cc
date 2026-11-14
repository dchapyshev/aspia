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

#include "client/storage/user_settings.h"

#include <QLocale>
#include <QVersionNumber>

#include <mutex>

#include "version.h"
#include "base/build_config.h"
#include "proto/peer.h"

namespace {

const QString kVersionParam = "version";
const QString kLocaleParam = "locale";
const QString kThemeParam = "theme";
const QString kLastDatabaseParam = "last_database";
const QString kSessionTypeParam = "session_type";
const QString kOneTimePasswordCheckedParam = "one_time_password_checked";
const QString kWindowGeometryParam = "window_geometry";
const QString kWindowStateParam = "window_state";
const QString kQuickConnectStateParam = "quick_connect_state";
const QString kDialogGeometryParam = "dialog_geometry";
const QString kLargeIconsParam = "large_icons";
const QString kToolbarParam = "toolbar";
const QString kSearchFieldParam = "search_field";
const QString kAlwaysOnTopParam = "always_on_top";
const QString kOpenSessionsInTabsParam = "open_sessions_in_tabs";
const QString kOnlineCheckParam = "online_check";
const QString kLastDirectoryParam = "last_directory";
const QString kConfirmCloseParam = "confirm_close";
const QString kCheckUpdatesParam = "check_updates";
const QString kUpdateChannelParam = "update_channel";
const QString kMcpEnabledParam = "mcp_enabled";
const QString kMcpAutoConfirmParam = "mcp_auto_confirm";
const QString kMcpSignInParam = "mcp_sign_in";
const QString kMcpSessionTypesParam = "mcp_session_types";

} // namespace

//--------------------------------------------------------------------------------------------------
UserSettings::UserSettings()
    : settings_(QSettings::IniFormat, QSettings::UserScope, "aspia", "client")
{
    static std::once_flag version_flag;
    std::call_once(version_flag, [this]()
    {
        const QVersionNumber current = QVersionNumber::fromString(ASPIA_VERSION_SHORT_STRING);
        if (QVersionNumber::fromString(settings_.value(kVersionParam).toString()) < current)
            settings_.setValue(kVersionParam, current.toString());
    });
}

//--------------------------------------------------------------------------------------------------
UserSettings::~UserSettings() = default;

//--------------------------------------------------------------------------------------------------
QString UserSettings::locale() const
{
    return settings_.value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLocale(const QString& locale)
{
    settings_.setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::theme() const
{
    return settings_.value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setTheme(const QString& theme)
{
    settings_.setValue(kThemeParam, theme);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::lastDatabase() const
{
    return settings_.value(kLastDatabaseParam).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLastDatabase(const QString& id)
{
    settings_.setValue(kLastDatabaseParam, id);
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType UserSettings::sessionType() const
{
    return static_cast<proto::peer::SessionType>(
        settings_.value(kSessionTypeParam, proto::peer::SESSION_TYPE_DESKTOP).toUInt());
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setSessionType(proto::peer::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, static_cast<quint32>(session_type));
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isOneTimePasswordChecked() const
{
    return settings_.value(kOneTimePasswordCheckedParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setOneTimePasswordChecked(bool check)
{
    settings_.setValue(kOneTimePasswordCheckedParam, check);
}

//--------------------------------------------------------------------------------------------------
QByteArray UserSettings::windowGeometry() const
{
    return settings_.value(kWindowGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(kWindowGeometryParam, geometry);
}

//--------------------------------------------------------------------------------------------------
QByteArray UserSettings::windowState() const
{
    return settings_.value(kWindowStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(kWindowStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray UserSettings::quickConnectState() const
{
    return settings_.value(kQuickConnectStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setQuickConnectState(const QByteArray& state)
{
    settings_.setValue(kQuickConnectStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray UserSettings::dialogGeometry(const QString& name) const
{
    return settings_.value(kDialogGeometryParam + "/" + name).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setDialogGeometry(const QString& name, const QByteArray& geometry)
{
    settings_.setValue(kDialogGeometryParam + "/" + name, geometry);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::largeIcons() const
{
    return settings_.value(kLargeIconsParam).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLargeIcons(bool enable)
{
    settings_.setValue(kLargeIconsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isToolBarEnabled() const
{
    return settings_.value(kToolbarParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setToolBarEnabled(bool enable)
{
    settings_.setValue(kToolbarParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isSearchFieldEnabled() const
{
    return settings_.value(kSearchFieldParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setSearchFieldEnabled(bool enable)
{
    settings_.setValue(kSearchFieldParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::alwaysOnTop() const
{
    return settings_.value(kAlwaysOnTopParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setAlwaysOnTop(bool enable)
{
    settings_.setValue(kAlwaysOnTopParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::openSessionsInTabs() const
{
    return settings_.value(kOpenSessionsInTabsParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setOpenSessionsInTabs(bool enable)
{
    settings_.setValue(kOpenSessionsInTabsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isOnlineCheckEnabled() const
{
    return settings_.value(kOnlineCheckParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setOnlineCheckEnabled(bool enable)
{
    settings_.setValue(kOnlineCheckParam, enable);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::lastDirectory() const
{
    return settings_.value(kLastDirectoryParam).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLastDirectory(const QString& path)
{
    settings_.setValue(kLastDirectoryParam, path);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isConfirmCloseEnabled() const
{
    return settings_.value(kConfirmCloseParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setConfirmCloseEnabled(bool enable)
{
    settings_.setValue(kConfirmCloseParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isCheckUpdatesEnabled() const
{
    return settings_.value(kCheckUpdatesParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setCheckUpdatesEnabled(bool enable)
{
    settings_.setValue(kCheckUpdatesParam, enable);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::updateChannel() const
{
    QString channel = settings_.value(kUpdateChannelParam).toString();
    if (channel.isEmpty())
        channel = kStableUpdateChannel;
    return channel;
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setUpdateChannel(const QString& channel)
{
    settings_.setValue(kUpdateChannelParam, channel);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isMcpEnabled() const
{
    return settings_.value(kMcpEnabledParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpEnabled(bool enable)
{
    settings_.setValue(kMcpEnabledParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isMcpAutoConfirmEnabled() const
{
    return settings_.value(kMcpAutoConfirmParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpAutoConfirmEnabled(bool enable)
{
    settings_.setValue(kMcpAutoConfirmParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isMcpSignInEnabled() const
{
    return settings_.value(kMcpSignInParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpSignInEnabled(bool enable)
{
    settings_.setValue(kMcpSignInParam, enable);
}

//--------------------------------------------------------------------------------------------------
quint32 UserSettings::mcpSessionTypes() const
{
    bool ok = false;
    const quint32 value = settings_.value(kMcpSessionTypesParam).toUInt(&ok);
    if (!ok)
        return proto::peer::SESSION_TYPE_ALL;

    return value;
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpSessionTypes(quint32 session_types)
{
    settings_.setValue(kMcpSessionTypesParam, session_types);
}
