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

#include "console/console_settings.h"

#include "build/build_config.h"
#include "qt_base/qt_xml_settings.h"

#include <QDir>
#include <QLocale>

namespace console {

Settings::Settings()
    : settings_(qt_base::QtXmlSettings::format(),
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("console"))
{
    // Nothing
}

QString Settings::locale() const
{
    return settings_.value(QStringLiteral("Locale"), QLocale::system().bcp47Name()).toString();
}

void Settings::setLocale(const QString& locale)
{
    settings_.setValue(QStringLiteral("Locale"), locale);
}

QString Settings::lastDirectory() const
{
    return settings_.value(QStringLiteral("LastDirectory"), QDir::homePath()).toString();
}

void Settings::setLastDirectory(const QString& directory_path)
{
    settings_.setValue(QStringLiteral("LastDirectory"), directory_path);
}

QByteArray Settings::windowGeometry() const
{
    return settings_.value(QStringLiteral("WindowGeometry")).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(QStringLiteral("WindowGeometry"), geometry);
}

QByteArray Settings::windowState() const
{
    return settings_.value(QStringLiteral("WindowState")).toByteArray();
}

void Settings::setWindowState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("WindowState"), state);
}

QByteArray Settings::splitterState() const
{
    return settings_.value(QStringLiteral("SplitterState")).toByteArray();
}

void Settings::setSplitterState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("SplitterState"), state);
}

QByteArray Settings::columnsState() const
{
    return settings_.value(QStringLiteral("ColumnsState")).toByteArray();
}

void Settings::setColumnsState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("ColumnsState"), state);
}

bool Settings::isRecentOpenEnabled() const
{
    return settings_.value(QStringLiteral("EnableRecentOpen"), true).toBool();
}

void Settings::setRecentOpenEnabled(bool enable)
{
    settings_.setValue(QStringLiteral("EnableRecentOpen"), enable);
}

QStringList Settings::recentOpen() const
{
    return settings_.value(QStringLiteral("RecentOpen")).toStringList();
}

void Settings::setRecentOpen(const QStringList& mru)
{
    settings_.setValue(QStringLiteral("RecentOpen"), mru);
}

QStringList Settings::pinnedFiles() const
{
    return settings_.value(QStringLiteral("PinnedFiles")).toStringList();
}

void Settings::setPinnedFiles(const QStringList& tabs)
{
    settings_.setValue(QStringLiteral("PinnedFiles"), tabs);
}

bool Settings::isToolBarEnabled() const
{
    return settings_.value(QStringLiteral("Toolbar"), true).toBool();
}

void Settings::setToolBarEnabled(bool enable)
{
    settings_.setValue(QStringLiteral("Toolbar"), enable);
}

bool Settings::isStatusBarEnabled() const
{
    return settings_.value(QStringLiteral("Statusbar"), true).toBool();
}

void Settings::setStatusBarEnabled(bool enable)
{
    settings_.setValue(QStringLiteral("Statusbar"), enable);
}

bool Settings::minimizeToTray() const
{
    return settings_.value(QStringLiteral("MinimizeToTray"), false).toBool();
}

void Settings::setMinimizeToTray(bool enable)
{
    settings_.setValue(QStringLiteral("MinimizeToTray"), enable);
}

bool Settings::alwaysShowTrayIcon() const
{
    return settings_.value(QStringLiteral("AlwaysShowTrayIcon"), false).toBool();
}

void Settings::setAlwaysShowTrayIcon(bool enable)
{
    settings_.setValue(QStringLiteral("AlwaysShowTrayIcon"), enable);
}

proto::SessionType Settings::sessionType() const
{
    return static_cast<proto::SessionType>(
        settings_.value(QStringLiteral("SessionType"),
                        proto::SESSION_TYPE_DESKTOP_MANAGE).toInt());
}

void Settings::setSessionType(proto::SessionType session_type)
{
    settings_.setValue(QStringLiteral("SessionType"), session_type);
}

bool Settings::checkUpdates() const
{
    return settings_.value(QStringLiteral("CheckUpdates"), true).toBool();
}

void Settings::setCheckUpdates(bool check)
{
    settings_.setValue(QStringLiteral("CheckUpdates"), check);
}

QString Settings::updateServer() const
{
    return settings_.value(QStringLiteral("UpdateServer"), DEFAULT_UPDATE_SERVER)
        .toString().toLower();
}

void Settings::setUpdateServer(const QString& server)
{
    settings_.setValue(QStringLiteral("UpdateServer"), server.toLower());
}

QByteArray Settings::computerDialogGeometry() const
{
    return settings_.value(QStringLiteral("ComputerDialogGeometry")).toByteArray();
}

void Settings::setComputerDialogGeometry(const QByteArray& geometry)
{
    settings_.setValue(QStringLiteral("ComputerDialogGeometry"), geometry);
}

QByteArray Settings::computerDialogState() const
{
    return settings_.value(QStringLiteral("ComputerDialogState")).toByteArray();
}

void Settings::setComputerDialogState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("ComputerDialogState"), state);
}

QByteArray Settings::computerGroupDialogGeometry() const
{
    return settings_.value(QStringLiteral("ComputerGroupDialogGeometry")).toByteArray();
}

void Settings::setComputerGroupDialogGeometry(const QByteArray& geometry)
{
    settings_.setValue(QStringLiteral("ComputerGroupDialogGeometry"), geometry);
}

} // namespace console
