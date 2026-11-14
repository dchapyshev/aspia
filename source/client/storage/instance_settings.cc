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

#include "client/storage/instance_settings.h"

#include <QFile>
#include <QVersionNumber>

#include <mutex>

#include "version.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "proto/peer.h"

namespace {

QString g_test_file_path;
QString g_selected_id;

const QString kDirectoryName = "client";

const QString kVersionParam = "version";
const QString kSessionTypeParam = "session_type";
const QString kOneTimePasswordCheckedParam = "one_time_password_checked";
const QString kQuickConnectStateParam = "quick_connect_state";
const QString kWindowGeometryParam = "window_geometry";
const QString kWindowStateParam = "window_state";
const QString kLargeIconsParam = "large_icons";
const QString kToolbarParam = "toolbar";
const QString kSearchFieldParam = "search_field";
const QString kOnlineCheckParam = "online_check";
const QString kAlwaysOnTopParam = "always_on_top";
const QString kOpenSessionsInTabsParam = "open_sessions_in_tabs";
const QString kLastDirectoryParam = "last_directory";
const QString kTabStateParam = "tab_state";
const QString kDialogGeometryParam = "dialog_geometry";
const QString kLocalGroupExpandedParam = "local_group_expanded";
const QString kWorkspaceExpandedParam = "workspace_expanded";
const QString kRouterGroupExpandedParam = "router_group_expanded";

//--------------------------------------------------------------------------------------------------
QString scopedKey(const QString& prefix, qint64 entry_id)
{
    return prefix + "/" + QString::number(entry_id);
}

//--------------------------------------------------------------------------------------------------
void writeExpanded(QSettings& settings, const QString& key, bool expanded, bool default_expanded)
{
    // Store only the deviation from the default to keep the file small.
    if (expanded == default_expanded)
        settings.remove(key);
    else
        settings.setValue(key, expanded);
}

} // namespace

//--------------------------------------------------------------------------------------------------
InstanceSettings::InstanceSettings()
    : settings_(filePath(), QSettings::IniFormat)
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
InstanceSettings::~InstanceSettings()
{
    settings_.setValue(kVersionParam, QString::fromLatin1(ASPIA_VERSION_SHORT_STRING));
}

//--------------------------------------------------------------------------------------------------
// static
void InstanceSettings::select(const QString& id)
{
    g_selected_id = id;
}

//--------------------------------------------------------------------------------------------------
// static
void InstanceSettings::remove(const QString& id)
{
    const QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty() || id.isEmpty())
        return;

    QFile::remove(dir_path + '/' + kDirectoryName + '/' + id + ".ini");
}

//--------------------------------------------------------------------------------------------------
// static
QString InstanceSettings::filePath()
{
    if (!g_test_file_path.isEmpty())
        return g_test_file_path;

    const QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty() || g_selected_id.isEmpty())
        return QString();

    return dir_path + '/' + kDirectoryName + '/' + g_selected_id + ".ini";
}

//--------------------------------------------------------------------------------------------------
// static
void InstanceSettings::setFilePathForTesting(const QString& file_path)
{
    g_test_file_path = file_path;
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType InstanceSettings::sessionType() const
{
    return static_cast<proto::peer::SessionType>(
        settings_.value(kSessionTypeParam, proto::peer::SESSION_TYPE_DESKTOP).toUInt());
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setSessionType(proto::peer::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, static_cast<quint32>(session_type));
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::isOneTimePasswordChecked() const
{
    return settings_.value(kOneTimePasswordCheckedParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setOneTimePasswordChecked(bool check)
{
    settings_.setValue(kOneTimePasswordCheckedParam, check);
}

//--------------------------------------------------------------------------------------------------
QByteArray InstanceSettings::quickConnectState() const
{
    return settings_.value(kQuickConnectStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setQuickConnectState(const QByteArray& state)
{
    settings_.setValue(kQuickConnectStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray InstanceSettings::windowGeometry() const
{
    return settings_.value(kWindowGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(kWindowGeometryParam, geometry);
}

//--------------------------------------------------------------------------------------------------
QByteArray InstanceSettings::windowState() const
{
    return settings_.value(kWindowStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(kWindowStateParam, state);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::largeIcons() const
{
    return settings_.value(kLargeIconsParam).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setLargeIcons(bool enable)
{
    settings_.setValue(kLargeIconsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::isToolBarEnabled() const
{
    return settings_.value(kToolbarParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setToolBarEnabled(bool enable)
{
    settings_.setValue(kToolbarParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::isSearchFieldEnabled() const
{
    return settings_.value(kSearchFieldParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setSearchFieldEnabled(bool enable)
{
    settings_.setValue(kSearchFieldParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::isOnlineCheckEnabled() const
{
    return settings_.value(kOnlineCheckParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setOnlineCheckEnabled(bool enable)
{
    settings_.setValue(kOnlineCheckParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::alwaysOnTop() const
{
    return settings_.value(kAlwaysOnTopParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setAlwaysOnTop(bool enable)
{
    settings_.setValue(kAlwaysOnTopParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::openSessionsInTabs() const
{
    return settings_.value(kOpenSessionsInTabsParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setOpenSessionsInTabs(bool enable)
{
    settings_.setValue(kOpenSessionsInTabsParam, enable);
}

//--------------------------------------------------------------------------------------------------
QString InstanceSettings::lastDirectory() const
{
    return settings_.value(kLastDirectoryParam).toString();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setLastDirectory(const QString& path)
{
    settings_.setValue(kLastDirectoryParam, path);
}

//--------------------------------------------------------------------------------------------------
QByteArray InstanceSettings::tabState(const QString& name) const
{
    return settings_.value(kTabStateParam + "/" + name).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setTabState(const QString& name, const QByteArray& state)
{
    settings_.setValue(kTabStateParam + "/" + name, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray InstanceSettings::dialogGeometry(const QString& name) const
{
    return settings_.value(kDialogGeometryParam + "/" + name).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setDialogGeometry(const QString& name, const QByteArray& geometry)
{
    settings_.setValue(kDialogGeometryParam + "/" + name, geometry);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::isLocalGroupExpanded(qint64 group_id) const
{
    return settings_.value(kLocalGroupExpandedParam + "/" + QString::number(group_id), true).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setLocalGroupExpanded(qint64 group_id, bool expanded)
{
    QString key = kLocalGroupExpandedParam + "/" + QString::number(group_id);

    // Default is true: store only the deviation from the default to keep the settings file small.
    if (expanded)
        settings_.remove(key);
    else
        settings_.setValue(key, false);
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::removeLocalGroupExpanded(qint64 group_id)
{
    settings_.remove(kLocalGroupExpandedParam + "/" + QString::number(group_id));
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::isWorkspaceExpanded(qint64 workspace_id) const
{
    return settings_.value(scopedKey(kWorkspaceExpandedParam, workspace_id), false).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setWorkspaceExpanded(qint64 workspace_id, bool expanded)
{
    writeExpanded(settings_, scopedKey(kWorkspaceExpandedParam, workspace_id), expanded, false);
}

//--------------------------------------------------------------------------------------------------
bool InstanceSettings::isRouterGroupExpanded(qint64 group_id) const
{
    return settings_.value(scopedKey(kRouterGroupExpandedParam, group_id), true).toBool();
}

//--------------------------------------------------------------------------------------------------
void InstanceSettings::setRouterGroupExpanded(qint64 group_id, bool expanded)
{
    writeExpanded(settings_, scopedKey(kRouterGroupExpandedParam, group_id), expanded, true);
}
