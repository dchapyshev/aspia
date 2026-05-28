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

#include "client/settings.h"

#include <QLocale>
#include <QStandardPaths>
#include <QVersionNumber>

#include <mutex>

#include "base/logging.h"
#include "base/xml_settings.h"
#include "build/version.h"
#include "client/config.h"
#include "proto/desktop_control.h"
#include "proto/peer.h"

namespace {

const QString kVersionParam = "version";
const QString kLocaleParam = "locale";
const QString kThemeParam = "theme";
const QString kSessionTypeParam = "session_type";
const QString kDesktopConfigParam = "desktop_config";
const QString kOneTimePasswordCheckedParam = "one_time_password_checked";
const QString kWindowGeometryParam = "window_geometry";
const QString kWindowStateParam = "window_state";
const QString kLargeIconsParam = "large_icons";
const QString kToolbarParam = "toolbar";
const QString kStatusbarParam = "statusbar";
const QString kOnlineCheckParam = "online_check";
const QString kAlwaysOnTopParam = "always_on_top";
const QString kOpenSessionsInTabsParam = "open_sessions_in_tabs";
const QString kRecordingPathParam = "recording_path";
const QString kRecordSessionsParam = "record_sessions";
const QString kSendKeyCombinationsParam = "send_key_combinations";
const QString kUdpAllowedParam = "udp_allowed";
const QString kTabStateParam = "tab_state";
const QString kLocalGroupExpandedParam = "local_group_expanded";
const QString kWorkspaceExpandedParam = "workspace_expanded";
const QString kRouterGroupExpandedParam = "router_group_expanded";

//--------------------------------------------------------------------------------------------------
QString scopedKey(const QString& prefix, qint64 router_id, qint64 entry_id)
{
    return prefix + "/" + QString::number(router_id) + "/" + QString::number(entry_id);
}

//--------------------------------------------------------------------------------------------------
void writeExpanded(QSettings& settings, const QString& key, bool expanded)
{
    // Default is true: store only the deviation from the default to keep the file small.
    if (expanded)
        settings.remove(key);
    else
        settings.setValue(key, false);
}

} // namespace

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    : settings_(XmlSettings::format(), QSettings::UserScope, "aspia", "client")
{
    static std::once_flag version_check_flag;
    std::call_once(version_check_flag, [this]()
    {
        QVersionNumber stored = QVersionNumber::fromString(settings_.value(kVersionParam).toString());
        QVersionNumber current = QVersionNumber::fromString(ASPIA_VERSION_SHORT_STRING);

        if (stored < current)
        {
            LOG(INFO) << "Settings version changed from" << stored.toString()
                      << "to" << current.toString();
            settings_.clear();
        }
    });
}

//--------------------------------------------------------------------------------------------------
Settings::~Settings()
{
    settings_.setValue(kVersionParam, QString::fromLatin1(ASPIA_VERSION_SHORT_STRING));
}

//--------------------------------------------------------------------------------------------------
QString Settings::locale() const
{
    return settings_.value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setLocale(const QString& locale)
{
    settings_.setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QString Settings::theme() const
{
    return settings_.value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setTheme(const QString& theme)
{
    settings_.setValue(kThemeParam, theme);
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType Settings::sessionType() const
{
    return static_cast<proto::peer::SessionType>(
        settings_.value(kSessionTypeParam, proto::peer::SESSION_TYPE_DESKTOP).toUInt());
}

//--------------------------------------------------------------------------------------------------
void Settings::setSessionType(proto::peer::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, static_cast<quint32>(session_type));
}

//--------------------------------------------------------------------------------------------------
proto::control::Config Settings::desktopConfig() const
{
    QByteArray buffer = settings_.value(kDesktopConfigParam).toByteArray();
    if (!buffer.isEmpty())
    {
        proto::control::Config config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return defaultDesktopConfig();
}

//--------------------------------------------------------------------------------------------------
void Settings::setDesktopConfig(const proto::control::Config& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    settings_.setValue(kDesktopConfigParam, buffer);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isOneTimePasswordChecked() const
{
    return settings_.value(kOneTimePasswordCheckedParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setOneTimePasswordChecked(bool check)
{
    settings_.setValue(kOneTimePasswordCheckedParam, check);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::windowGeometry() const
{
    return settings_.value(kWindowGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(kWindowGeometryParam, geometry);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::windowState() const
{
    return settings_.value(kWindowStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setWindowState(const QByteArray& state)
{
    settings_.setValue(kWindowStateParam, state);
}

//--------------------------------------------------------------------------------------------------
bool Settings::largeIcons() const
{
    return settings_.value(kLargeIconsParam).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setLargeIcons(bool enable)
{
    settings_.setValue(kLargeIconsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isToolBarEnabled() const
{
    return settings_.value(kToolbarParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setToolBarEnabled(bool enable)
{
    settings_.setValue(kToolbarParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isStatusBarEnabled() const
{
    return settings_.value(kStatusbarParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setStatusBarEnabled(bool enable)
{
    settings_.setValue(kStatusbarParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isOnlineCheckEnabled() const
{
    return settings_.value(kOnlineCheckParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setOnlineCheckEnabled(bool enable)
{
    settings_.setValue(kOnlineCheckParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::alwaysOnTop() const
{
    return settings_.value(kAlwaysOnTopParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setAlwaysOnTop(bool enable)
{
    settings_.setValue(kAlwaysOnTopParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::openSessionsInTabs() const
{
    return settings_.value(kOpenSessionsInTabsParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setOpenSessionsInTabs(bool enable)
{
    settings_.setValue(kOpenSessionsInTabsParam, enable);
}

//--------------------------------------------------------------------------------------------------
QString Settings::recordingPath() const
{
    QString default_path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/Aspia";
    return settings_.value(kRecordingPathParam, default_path).toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRecordingPath(const QString& path)
{
    settings_.setValue(kRecordingPathParam, path);
}

//--------------------------------------------------------------------------------------------------
bool Settings::recordSessions() const
{
    return settings_.value(kRecordSessionsParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRecordSessions(bool enable)
{
    settings_.setValue(kRecordSessionsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::sendKeyCombinations() const
{
    return settings_.value(kSendKeyCombinationsParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setSendKeyCombinations(bool enable)
{
    settings_.setValue(kSendKeyCombinationsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isUdpAllowed() const
{
    return settings_.value(kUdpAllowedParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setUdpAllowed(bool enable)
{
    settings_.setValue(kUdpAllowedParam, enable);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::tabState(const QString& name) const
{
    return settings_.value(kTabStateParam + "/" + name).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setTabState(const QString& name, const QByteArray& state)
{
    settings_.setValue(kTabStateParam + "/" + name, state);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isLocalGroupExpanded(qint64 group_id) const
{
    return settings_.value(kLocalGroupExpandedParam + "/" + QString::number(group_id), true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setLocalGroupExpanded(qint64 group_id, bool expanded)
{
    QString key = kLocalGroupExpandedParam + "/" + QString::number(group_id);

    // Default is true: store only the deviation from the default to keep the settings file small.
    if (expanded)
        settings_.remove(key);
    else
        settings_.setValue(key, false);
}

//--------------------------------------------------------------------------------------------------
void Settings::removeLocalGroupExpanded(qint64 group_id)
{
    settings_.remove(kLocalGroupExpandedParam + "/" + QString::number(group_id));
}

//--------------------------------------------------------------------------------------------------
bool Settings::isWorkspaceExpanded(qint64 router_id, qint64 workspace_id) const
{
    return settings_.value(scopedKey(kWorkspaceExpandedParam, router_id, workspace_id), true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setWorkspaceExpanded(qint64 router_id, qint64 workspace_id, bool expanded)
{
    writeExpanded(settings_, scopedKey(kWorkspaceExpandedParam, router_id, workspace_id), expanded);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isRouterGroupExpanded(qint64 router_id, qint64 group_id) const
{
    return settings_.value(scopedKey(kRouterGroupExpandedParam, router_id, group_id), true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterGroupExpanded(qint64 router_id, qint64 group_id, bool expanded)
{
    writeExpanded(settings_, scopedKey(kRouterGroupExpandedParam, router_id, group_id), expanded);
}

//--------------------------------------------------------------------------------------------------
void Settings::removeRouterExpanded(qint64 router_id)
{
    const QString router_suffix = "/" + QString::number(router_id);
    settings_.remove(kWorkspaceExpandedParam + router_suffix);
    settings_.remove(kRouterGroupExpandedParam + router_suffix);
}
