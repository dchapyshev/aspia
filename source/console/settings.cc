//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QDir>
#include <QLocale>

#include "base/xml_settings.h"
#include "build/build_config.h"

namespace console {

namespace {

const QString kLocaleParam = QStringLiteral("Locale");
const QString kLastDirectoryParam = QStringLiteral("LastDirectory");
const QString kWindowGeometryParam = QStringLiteral("WindowGeometry");
const QString kWindowStateParam = QStringLiteral("WindowState");
const QString kAddressBookStateParam = QStringLiteral("AddressBookState");
const QString kEnableRecentOpenParam = QStringLiteral("EnableRecentOpen");
const QString kRecentOpenParam = QStringLiteral("RecentOpen");
const QString kPinnedFilesParam = QStringLiteral("PinnedFiles");
const QString kToolbarParam = QStringLiteral("Toolbar");
const QString kStatusbarParam = QStringLiteral("Statusbar");
const QString kMinimizeToTrayParam = QStringLiteral("MinimizeToTray");
const QString kAlwaysShowTrayIconParam = QStringLiteral("AlwaysShowTrayIcon");
const QString kSessionTypeParam = QStringLiteral("SessionType");
const QString kCheckUpdatesParam = QStringLiteral("CheckUpdates");
const QString kUpdateServerParam = QStringLiteral("UpdateServer");
const QString kComputerDialogGeometryParam = QStringLiteral("ComputerDialogGeometry");
const QString kComputerDialogStateParam = QStringLiteral("ComputerDialogState");
const QString kComputerGroupDialogGeometryParam = QStringLiteral("ComputerGroupDialogGeometry");
const QString kShowIconsInMenusParam = QStringLiteral("ShowIconsInMenus");
const QString kAddressBookDialogGeometryParam = QStringLiteral("AddressBookDialogGeometry");
const QString kLargeIconsParam = QStringLiteral("LargeIcons");

} // namespace

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    : settings_(base::XmlSettings::format(),
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("console"))
{
    // Nothing
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
QString Settings::lastDirectory() const
{
    return settings_.value(kLastDirectoryParam, QDir::homePath()).toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setLastDirectory(const QString& directory_path)
{
    settings_.setValue(kLastDirectoryParam, directory_path);
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
QByteArray Settings::addressBookState() const
{
    return settings_.value(kAddressBookStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setAddressBookState(const QByteArray& state)
{
    settings_.setValue(kAddressBookStateParam, state);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isRecentOpenEnabled() const
{
    return settings_.value(kEnableRecentOpenParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRecentOpenEnabled(bool enable)
{
    settings_.setValue(kEnableRecentOpenParam, enable);
}

//--------------------------------------------------------------------------------------------------
QStringList Settings::recentOpen() const
{
    return settings_.value(kRecentOpenParam).toStringList();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRecentOpen(const QStringList& mru)
{
    settings_.setValue(kRecentOpenParam, mru);
}

//--------------------------------------------------------------------------------------------------
QStringList Settings::pinnedFiles() const
{
    return settings_.value(kPinnedFilesParam).toStringList();
}

//--------------------------------------------------------------------------------------------------
void Settings::setPinnedFiles(const QStringList& tabs)
{
    settings_.setValue(kPinnedFilesParam, tabs);
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
bool Settings::minimizeToTray() const
{
    return settings_.value(kMinimizeToTrayParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setMinimizeToTray(bool enable)
{
    settings_.setValue(kMinimizeToTrayParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::alwaysShowTrayIcon() const
{
    return settings_.value(kAlwaysShowTrayIconParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setAlwaysShowTrayIcon(bool enable)
{
    settings_.setValue(kAlwaysShowTrayIconParam, enable);
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType Settings::sessionType() const
{
    return static_cast<proto::peer::SessionType>(
        settings_.value(kSessionTypeParam, proto::peer::SESSION_TYPE_DESKTOP_MANAGE).toInt());
}

//--------------------------------------------------------------------------------------------------
void Settings::setSessionType(proto::peer::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, session_type);
}

//--------------------------------------------------------------------------------------------------
bool Settings::checkUpdates() const
{
    return settings_.value(kCheckUpdatesParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setCheckUpdates(bool check)
{
    settings_.setValue(kCheckUpdatesParam, check);
}

//--------------------------------------------------------------------------------------------------
QString Settings::updateServer() const
{
    return settings_.value(kUpdateServerParam, DEFAULT_UPDATE_SERVER).toString().toLower();
}

//--------------------------------------------------------------------------------------------------
void Settings::setUpdateServer(const QString& server)
{
    settings_.setValue(kUpdateServerParam, server.toLower());
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::computerDialogGeometry() const
{
    return settings_.value(kComputerDialogGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setComputerDialogGeometry(const QByteArray& geometry)
{
    settings_.setValue(kComputerDialogGeometryParam, geometry);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::computerDialogState() const
{
    return settings_.value(kComputerDialogStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setComputerDialogState(const QByteArray& state)
{
    settings_.setValue(kComputerDialogStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::computerGroupDialogGeometry() const
{
    return settings_.value(kComputerGroupDialogGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setComputerGroupDialogGeometry(const QByteArray& geometry)
{
    settings_.setValue(kComputerGroupDialogGeometryParam, geometry);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::fastConnectConfig(const QString& guid)
{
    return settings_.value(QStringLiteral("FastConnect/") + guid).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setFastConnectConfig(const QString& guid, const QByteArray& config)
{
    settings_.setValue(QStringLiteral("FastConnect/") + guid, config);
}

//--------------------------------------------------------------------------------------------------
bool Settings::showIconsInMenus() const
{
    bool defaultValue;

#if defined(Q_OS_MACOS)
    defaultValue = false;
#else
    defaultValue = true;
#endif

    return settings_.value(kShowIconsInMenusParam, defaultValue).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setShowIconsInMenus(bool enable)
{
    settings_.setValue(kShowIconsInMenusParam, enable);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::addressBookDialogGeometry() const
{
    return settings_.value(kAddressBookDialogGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setAddressBookDialogGeometry(const QByteArray& geometry)
{
    settings_.setValue(kAddressBookDialogGeometryParam, geometry);
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

} // namespace console
