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

namespace {

QString g_test_file_path;
QString g_selected_id;

const QString kDirectoryName = "client";

const QString kVersionParam = "version";
const QString kQuickConnectStateParam = "quick_connect_state";
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
