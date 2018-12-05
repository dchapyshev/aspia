//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/console_settings.h"

#include <QDir>

namespace aspia {

ConsoleSettings::ConsoleSettings()
    : settings_(QSettings::UserScope, QStringLiteral("aspia"), QStringLiteral("console"))
{
    // Nothing
}

// static
QString ConsoleSettings::defaultLocale()
{
    return QStringLiteral("en");
}

QString ConsoleSettings::locale() const
{
    return settings_.value(QStringLiteral("locale"), defaultLocale()).toString();
}

void ConsoleSettings::setLocale(const QString& locale)
{
    settings_.setValue(QStringLiteral("locale"), locale);
}

QString ConsoleSettings::lastDirectory() const
{
    return settings_.value(QStringLiteral("last_dir"), QDir::homePath()).toString();
}

void ConsoleSettings::setLastDirectory(const QString& directory_path)
{
    settings_.setValue(QStringLiteral("last_dir"), directory_path);
}

QByteArray ConsoleSettings::windowGeometry() const
{
    return settings_.value(QStringLiteral("window_geometry")).toByteArray();
}

void ConsoleSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(QStringLiteral("window_geometry"), geometry);
}

QByteArray ConsoleSettings::windowState() const
{
    return settings_.value(QStringLiteral("window_state")).toByteArray();
}

void ConsoleSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("window_state"), state);
}

QByteArray ConsoleSettings::splitterState() const
{
    return settings_.value(QStringLiteral("splitter_state")).toByteArray();
}

void ConsoleSettings::setSplitterState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("splitter_state"), state);
}

QByteArray ConsoleSettings::columnsState() const
{
    return settings_.value(QStringLiteral("columns_state")).toByteArray();
}

void ConsoleSettings::setColumnsState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("columns_state"), state);
}

QStringList ConsoleSettings::recentOpen() const
{
    return settings_.value(QStringLiteral("recent_open")).toStringList();
}

void ConsoleSettings::setRecentOpen(const QStringList& mru)
{
    settings_.setValue(QStringLiteral("recent_open"), mru);
}

QStringList ConsoleSettings::pinnedFiles() const
{
    return settings_.value(QStringLiteral("pinned_files")).toStringList();
}

void ConsoleSettings::setPinnedFiles(const QStringList& tabs)
{
    settings_.setValue(QStringLiteral("pinned_files"), tabs);
}

bool ConsoleSettings::isToolBarEnabled() const
{
    return settings_.value(QStringLiteral("toolbar"), true).toBool();
}

void ConsoleSettings::setToolBarEnabled(bool enable)
{
    settings_.setValue(QStringLiteral("toolbar"), enable);
}

bool ConsoleSettings::isStatusBarEnabled() const
{
    return settings_.value(QStringLiteral("statusbar"), true).toBool();
}

void ConsoleSettings::setStatusBarEnabled(bool enable)
{
    settings_.setValue(QStringLiteral("statusbar"), enable);
}

bool ConsoleSettings::minimizeToTray() const
{
    return settings_.value(QStringLiteral("minimize_to_tray"), false).toBool();
}

void ConsoleSettings::setMinimizeToTray(bool enable)
{
    settings_.setValue(QStringLiteral("minimize_to_tray"), enable);
}

bool ConsoleSettings::alwaysShowTrayIcon() const
{
    return settings_.value(QStringLiteral("always_show_tray_icon"), false).toBool();
}

void ConsoleSettings::setAlwaysShowTrayIcon(bool enable)
{
    settings_.setValue(QStringLiteral("always_show_tray_icon"), enable);
}

proto::SessionType ConsoleSettings::sessionType() const
{
    return static_cast<proto::SessionType>(
        settings_.value(QStringLiteral("session_type"),
                        proto::SESSION_TYPE_DESKTOP_MANAGE).toInt());
}

void ConsoleSettings::setSessionType(proto::SessionType session_type)
{
    settings_.setValue(QStringLiteral("session_type"), session_type);
}

bool ConsoleSettings::checkUpdates() const
{
    return settings_.value(QStringLiteral("check_updates"), true).toBool();
}

void ConsoleSettings::setCheckUpdates(bool check)
{
    settings_.setValue(QStringLiteral("check_updates"), check);
}

} // namespace aspia
