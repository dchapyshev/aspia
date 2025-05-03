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

#include "client/ui/client_settings.h"

#include "base/xml_settings.h"
#include "build/build_config.h"
#include "client/config_factory.h"

#include <QLocale>

namespace client {

namespace {

const QString kLocaleParam = QStringLiteral("Locale");
const QString kAddressListParam = QStringLiteral("AddressList");
const QString kSessionTypeParam = QStringLiteral("SessionType");
const QString kDesktopManageConfigParam = QStringLiteral("DesktopManageConfig");
const QString kDesktopViewConfigParam = QStringLiteral("DesktopViewConfig");
const QString kShowIconsInMenusParam = QStringLiteral("ShowIconsInMenus");
const QString kCheckUpdatesParam = QStringLiteral("CheckUpdates");
const QString kUpdateServerParam = QStringLiteral("UpdateServer");
const QString kOneTimePasswordCheckedParam = QStringLiteral("OneTimePasswordChecked");
const QString kRouterManagerStateParam = QStringLiteral("RouterManagerState");
const QString kDisplayNameParam = QStringLiteral("DisplayName");

} // namespace

//--------------------------------------------------------------------------------------------------
ClientSettings::ClientSettings()
    : settings_(base::XmlSettings::format(),
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("client"))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString ClientSettings::locale() const
{
    return settings_.value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setLocale(const QString& locale)
{
    settings_.setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QStringList ClientSettings::addressList() const
{
    return settings_.value(kAddressListParam).toStringList();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setAddressList(const QStringList& list)
{
    settings_.setValue(kAddressListParam, list);
}

//--------------------------------------------------------------------------------------------------
proto::SessionType ClientSettings::sessionType() const
{
    return static_cast<proto::SessionType>(
        settings_.value(kSessionTypeParam, proto::SESSION_TYPE_DESKTOP_MANAGE).toUInt());
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setSessionType(proto::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, static_cast<quint32>(session_type));
}

//--------------------------------------------------------------------------------------------------
proto::DesktopConfig ClientSettings::desktopManageConfig() const
{
    QByteArray buffer = settings_.value(kDesktopManageConfigParam).toByteArray();
    if (!buffer.isEmpty())
    {
        proto::DesktopConfig config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopManageConfig();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setDesktopManageConfig(const proto::DesktopConfig& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    settings_.setValue(kDesktopManageConfigParam, buffer);
}

//--------------------------------------------------------------------------------------------------
proto::DesktopConfig ClientSettings::desktopViewConfig() const
{
    QByteArray buffer = settings_.value(kDesktopViewConfigParam).toByteArray();
    if (!buffer.isEmpty())
    {
        proto::DesktopConfig config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopViewConfig();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setDesktopViewConfig(const proto::DesktopConfig& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    settings_.setValue(kDesktopViewConfigParam, buffer);
}

//--------------------------------------------------------------------------------------------------
bool ClientSettings::showIconsInMenus() const
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
void ClientSettings::setShowIconsInMenus(bool enable)
{
    settings_.setValue(kShowIconsInMenusParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool ClientSettings::checkUpdates() const
{
    return settings_.value(kCheckUpdatesParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setCheckUpdates(bool check)
{
    settings_.setValue(kCheckUpdatesParam, check);
}

//--------------------------------------------------------------------------------------------------
QString ClientSettings::updateServer() const
{
    return settings_.value(kUpdateServerParam, DEFAULT_UPDATE_SERVER).toString().toLower();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setUpdateServer(const QString& server)
{
    settings_.setValue(kUpdateServerParam, server.toLower());
}

//--------------------------------------------------------------------------------------------------
bool ClientSettings::isOneTimePasswordChecked() const
{
    return settings_.value(kOneTimePasswordCheckedParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setOneTimePasswordChecked(bool check)
{
    settings_.setValue(kOneTimePasswordCheckedParam, check);
}

//--------------------------------------------------------------------------------------------------
QByteArray ClientSettings::routerManagerState() const
{
    return settings_.value(kRouterManagerStateParam, false).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setRouterManagerState(const QByteArray& state)
{
    settings_.setValue(kRouterManagerStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QString ClientSettings::displayName() const
{
    return settings_.value(kDisplayNameParam).toString();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setDisplayName(const QString& display_name)
{
    settings_.setValue(kDisplayNameParam, display_name);
}

} // namespace client
