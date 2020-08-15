//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/settings.h"

#include "build/build_config.h"

#include <QDir>
#include <QLocale>

namespace console {

Settings::Settings()
    : settings_(QSettings::IniFormat,
                QSettings::UserScope,
                "aspia",
                "console")
{
    // Nothing
}

QString Settings::locale() const
{
    return settings_.value("Locale", QLocale::system().bcp47Name()).toString();
}

void Settings::setLocale(const QString& locale)
{
    settings_.setValue("Locale", locale);
}

QString Settings::lastDirectory() const
{
    return settings_.value("LastDirectory", QDir::homePath()).toString();
}

void Settings::setLastDirectory(const QString& directory_path)
{
    settings_.setValue("LastDirectory", directory_path);
}

QByteArray Settings::windowGeometry() const
{
    return settings_.value("WindowGeometry").toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue("WindowGeometry", geometry);
}

QByteArray Settings::windowState() const
{
    return settings_.value("WindowState").toByteArray();
}

void Settings::setWindowState(const QByteArray& state)
{
    settings_.setValue("WindowState", state);
}

QByteArray Settings::splitterState() const
{
    return settings_.value("SplitterState").toByteArray();
}

void Settings::setSplitterState(const QByteArray& state)
{
    settings_.setValue("SplitterState", state);
}

QByteArray Settings::columnsState() const
{
    return settings_.value("ColumnsState").toByteArray();
}

void Settings::setColumnsState(const QByteArray& state)
{
    settings_.setValue("ColumnsState", state);
}

bool Settings::isRecentOpenEnabled() const
{
    return settings_.value("EnableRecentOpen", true).toBool();
}

void Settings::setRecentOpenEnabled(bool enable)
{
    settings_.setValue("EnableRecentOpen", enable);
}

QStringList Settings::recentOpen() const
{
    return settings_.value("RecentOpen").toStringList();
}

void Settings::setRecentOpen(const QStringList& mru)
{
    settings_.setValue("RecentOpen", mru);
}

QStringList Settings::pinnedFiles() const
{
    return settings_.value("PinnedFiles").toStringList();
}

void Settings::setPinnedFiles(const QStringList& tabs)
{
    settings_.setValue("PinnedFiles", tabs);
}

bool Settings::isToolBarEnabled() const
{
    return settings_.value("Toolbar", true).toBool();
}

void Settings::setToolBarEnabled(bool enable)
{
    settings_.setValue("Toolbar", enable);
}

bool Settings::isStatusBarEnabled() const
{
    return settings_.value("Statusbar", true).toBool();
}

void Settings::setStatusBarEnabled(bool enable)
{
    settings_.setValue("Statusbar", enable);
}

bool Settings::minimizeToTray() const
{
    return settings_.value("MinimizeToTray", false).toBool();
}

void Settings::setMinimizeToTray(bool enable)
{
    settings_.setValue("MinimizeToTray", enable);
}

bool Settings::alwaysShowTrayIcon() const
{
    return settings_.value("AlwaysShowTrayIcon", false).toBool();
}

void Settings::setAlwaysShowTrayIcon(bool enable)
{
    settings_.setValue("AlwaysShowTrayIcon", enable);
}

proto::SessionType Settings::sessionType() const
{
    return static_cast<proto::SessionType>(
        settings_.value("SessionType", proto::SESSION_TYPE_DESKTOP_MANAGE).toInt());
}

void Settings::setSessionType(proto::SessionType session_type)
{
    settings_.setValue("SessionType", session_type);
}

bool Settings::checkUpdates() const
{
    return settings_.value("CheckUpdates", true).toBool();
}

void Settings::setCheckUpdates(bool check)
{
    settings_.setValue("CheckUpdates", check);
}

QString Settings::updateServer() const
{
    return settings_.value("UpdateServer", DEFAULT_UPDATE_SERVER)
        .toString().toLower();
}

void Settings::setUpdateServer(const QString& server)
{
    settings_.setValue("UpdateServer", server.toLower());
}

QByteArray Settings::computerDialogGeometry() const
{
    return settings_.value("ComputerDialogGeometry").toByteArray();
}

void Settings::setComputerDialogGeometry(const QByteArray& geometry)
{
    settings_.setValue("ComputerDialogGeometry", geometry);
}

QByteArray Settings::computerDialogState() const
{
    return settings_.value("ComputerDialogState").toByteArray();
}

void Settings::setComputerDialogState(const QByteArray& state)
{
    settings_.setValue("ComputerDialogState", state);
}

QByteArray Settings::computerGroupDialogGeometry() const
{
    return settings_.value("ComputerGroupDialogGeometry").toByteArray();
}

void Settings::setComputerGroupDialogGeometry(const QByteArray& geometry)
{
    settings_.setValue("ComputerGroupDialogGeometry", geometry);
}

} // namespace console
