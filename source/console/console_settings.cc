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
#include <QLocale>

#include "base/xml_settings.h"
#include "build/build_config.h"

namespace aspia {

ConsoleSettings::ConsoleSettings()
    : settings_(base::XmlSettings::registerFormat(),
                QSettings::UserScope,
                QLatin1String("aspia"),
                QLatin1String("console"))
{
    // Nothing
}

QString ConsoleSettings::locale() const
{
    return settings_.value(QLatin1String("Locale"), QLocale::system().bcp47Name()).toString();
}

void ConsoleSettings::setLocale(const QString& locale)
{
    settings_.setValue(QLatin1String("Locale"), locale);
}

QString ConsoleSettings::lastDirectory() const
{
    return settings_.value(QLatin1String("LastDirectory"), QDir::homePath()).toString();
}

void ConsoleSettings::setLastDirectory(const QString& directory_path)
{
    settings_.setValue(QLatin1String("LastDirectory"), directory_path);
}

QByteArray ConsoleSettings::windowGeometry() const
{
    return settings_.value(QLatin1String("WindowGeometry")).toByteArray();
}

void ConsoleSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(QLatin1String("WindowGeometry"), geometry);
}

QByteArray ConsoleSettings::windowState() const
{
    return settings_.value(QLatin1String("WindowState")).toByteArray();
}

void ConsoleSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(QLatin1String("WindowState"), state);
}

QByteArray ConsoleSettings::splitterState() const
{
    return settings_.value(QLatin1String("SplitterState")).toByteArray();
}

void ConsoleSettings::setSplitterState(const QByteArray& state)
{
    settings_.setValue(QLatin1String("SplitterState"), state);
}

QByteArray ConsoleSettings::columnsState() const
{
    return settings_.value(QLatin1String("ColumnsState")).toByteArray();
}

void ConsoleSettings::setColumnsState(const QByteArray& state)
{
    settings_.setValue(QLatin1String("ColumnsState"), state);
}

QStringList ConsoleSettings::recentOpen() const
{
    return settings_.value(QLatin1String("RecentOpen")).toStringList();
}

void ConsoleSettings::setRecentOpen(const QStringList& mru)
{
    settings_.setValue(QLatin1String("RecentOpen"), mru);
}

QStringList ConsoleSettings::pinnedFiles() const
{
    return settings_.value(QLatin1String("PinnedFiles")).toStringList();
}

void ConsoleSettings::setPinnedFiles(const QStringList& tabs)
{
    settings_.setValue(QLatin1String("PinnedFiles"), tabs);
}

bool ConsoleSettings::isToolBarEnabled() const
{
    return settings_.value(QLatin1String("Toolbar"), true).toBool();
}

void ConsoleSettings::setToolBarEnabled(bool enable)
{
    settings_.setValue(QLatin1String("Toolbar"), enable);
}

bool ConsoleSettings::isStatusBarEnabled() const
{
    return settings_.value(QLatin1String("Statusbar"), true).toBool();
}

void ConsoleSettings::setStatusBarEnabled(bool enable)
{
    settings_.setValue(QLatin1String("Statusbar"), enable);
}

bool ConsoleSettings::minimizeToTray() const
{
    return settings_.value(QLatin1String("MinimizeToTray"), false).toBool();
}

void ConsoleSettings::setMinimizeToTray(bool enable)
{
    settings_.setValue(QLatin1String("MinimizeToTray"), enable);
}

bool ConsoleSettings::alwaysShowTrayIcon() const
{
    return settings_.value(QLatin1String("AlwaysShowTrayIcon"), false).toBool();
}

void ConsoleSettings::setAlwaysShowTrayIcon(bool enable)
{
    settings_.setValue(QLatin1String("AlwaysShowTrayIcon"), enable);
}

proto::SessionType ConsoleSettings::sessionType() const
{
    return static_cast<proto::SessionType>(
        settings_.value(QLatin1String("SessionType"),
                        proto::SESSION_TYPE_DESKTOP_MANAGE).toInt());
}

void ConsoleSettings::setSessionType(proto::SessionType session_type)
{
    settings_.setValue(QLatin1String("SessionType"), session_type);
}

bool ConsoleSettings::checkUpdates() const
{
    return settings_.value(QLatin1String("CheckUpdates"), true).toBool();
}

void ConsoleSettings::setCheckUpdates(bool check)
{
    settings_.setValue(QLatin1String("CheckUpdates"), check);
}

QString ConsoleSettings::updateServer() const
{
    return settings_.value(QLatin1String("UpdateServer"), DEFAULT_UPDATE_SERVER)
        .toString().toLower();
}

void ConsoleSettings::setUpdateServer(const QString& server)
{
    settings_.setValue(QLatin1String("UpdateServer"), server.toLower());
}

} // namespace aspia
