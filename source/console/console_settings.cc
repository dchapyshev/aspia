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

namespace console {

Settings::Settings()
    : settings_(base::XmlSettings::registerFormat(),
                QSettings::UserScope,
                QLatin1String("aspia"),
                QLatin1String("console"))
{
    // Nothing
}

QString Settings::locale() const
{
    return settings_.value(QLatin1String("Locale"), QLocale::system().bcp47Name()).toString();
}

void Settings::setLocale(const QString& locale)
{
    settings_.setValue(QLatin1String("Locale"), locale);
}

QString Settings::lastDirectory() const
{
    return settings_.value(QLatin1String("LastDirectory"), QDir::homePath()).toString();
}

void Settings::setLastDirectory(const QString& directory_path)
{
    settings_.setValue(QLatin1String("LastDirectory"), directory_path);
}

QByteArray Settings::windowGeometry() const
{
    return settings_.value(QLatin1String("WindowGeometry")).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(QLatin1String("WindowGeometry"), geometry);
}

QByteArray Settings::windowState() const
{
    return settings_.value(QLatin1String("WindowState")).toByteArray();
}

void Settings::setWindowState(const QByteArray& state)
{
    settings_.setValue(QLatin1String("WindowState"), state);
}

QByteArray Settings::splitterState() const
{
    return settings_.value(QLatin1String("SplitterState")).toByteArray();
}

void Settings::setSplitterState(const QByteArray& state)
{
    settings_.setValue(QLatin1String("SplitterState"), state);
}

QByteArray Settings::columnsState() const
{
    return settings_.value(QLatin1String("ColumnsState")).toByteArray();
}

void Settings::setColumnsState(const QByteArray& state)
{
    settings_.setValue(QLatin1String("ColumnsState"), state);
}

QStringList Settings::recentOpen() const
{
    return settings_.value(QLatin1String("RecentOpen")).toStringList();
}

void Settings::setRecentOpen(const QStringList& mru)
{
    settings_.setValue(QLatin1String("RecentOpen"), mru);
}

QStringList Settings::pinnedFiles() const
{
    return settings_.value(QLatin1String("PinnedFiles")).toStringList();
}

void Settings::setPinnedFiles(const QStringList& tabs)
{
    settings_.setValue(QLatin1String("PinnedFiles"), tabs);
}

bool Settings::isToolBarEnabled() const
{
    return settings_.value(QLatin1String("Toolbar"), true).toBool();
}

void Settings::setToolBarEnabled(bool enable)
{
    settings_.setValue(QLatin1String("Toolbar"), enable);
}

bool Settings::isStatusBarEnabled() const
{
    return settings_.value(QLatin1String("Statusbar"), true).toBool();
}

void Settings::setStatusBarEnabled(bool enable)
{
    settings_.setValue(QLatin1String("Statusbar"), enable);
}

bool Settings::minimizeToTray() const
{
    return settings_.value(QLatin1String("MinimizeToTray"), false).toBool();
}

void Settings::setMinimizeToTray(bool enable)
{
    settings_.setValue(QLatin1String("MinimizeToTray"), enable);
}

bool Settings::alwaysShowTrayIcon() const
{
    return settings_.value(QLatin1String("AlwaysShowTrayIcon"), false).toBool();
}

void Settings::setAlwaysShowTrayIcon(bool enable)
{
    settings_.setValue(QLatin1String("AlwaysShowTrayIcon"), enable);
}

proto::SessionType Settings::sessionType() const
{
    return static_cast<proto::SessionType>(
        settings_.value(QLatin1String("SessionType"),
                        proto::SESSION_TYPE_DESKTOP_MANAGE).toInt());
}

void Settings::setSessionType(proto::SessionType session_type)
{
    settings_.setValue(QLatin1String("SessionType"), session_type);
}

bool Settings::checkUpdates() const
{
    return settings_.value(QLatin1String("CheckUpdates"), true).toBool();
}

void Settings::setCheckUpdates(bool check)
{
    settings_.setValue(QLatin1String("CheckUpdates"), check);
}

QString Settings::updateServer() const
{
    return settings_.value(QLatin1String("UpdateServer"), DEFAULT_UPDATE_SERVER)
        .toString().toLower();
}

void Settings::setUpdateServer(const QString& server)
{
    settings_.setValue(QLatin1String("UpdateServer"), server.toLower());
}

} // namespace console
