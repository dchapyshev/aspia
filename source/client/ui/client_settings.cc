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

#include "client/ui/client_settings.h"

#include "base/xml_settings.h"
#include "build/build_config.h"
#include "client/config_factory.h"

#include <QLocale>

namespace client {

namespace {

const QString kLocaleParam = "Locale";
const QString kThemeParam = "Theme";
const QString kAddressListParam = "AddressList";
const QString kSessionTypeParam = "SessionType";
const QString kDesktopManageConfigParam = "DesktopManageConfig";
const QString kDesktopViewConfigParam = "DesktopViewConfig";
const QString kCheckUpdatesParam = "CheckUpdates";
const QString kUpdateServerParam = "UpdateServer";
const QString kOneTimePasswordCheckedParam = "OneTimePasswordChecked";
const QString kRouterManagerStateParam = "RouterManagerState";
const QString kDisplayNameParam = "DisplayName";

} // namespace

//--------------------------------------------------------------------------------------------------
ClientSettings::ClientSettings()
    : settings_(base::XmlSettings::format(), QSettings::UserScope, "aspia", "client")
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
QString ClientSettings::theme() const
{
    return settings_.value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setTheme(const QString& theme)
{
    settings_.setValue(kThemeParam, theme);
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
proto::peer::SessionType ClientSettings::sessionType() const
{
    return static_cast<proto::peer::SessionType>(
        settings_.value(kSessionTypeParam, proto::peer::SESSION_TYPE_DESKTOP_MANAGE).toUInt());
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setSessionType(proto::peer::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, static_cast<quint32>(session_type));
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Config ClientSettings::desktopManageConfig() const
{
    QByteArray buffer = settings_.value(kDesktopManageConfigParam).toByteArray();
    if (!buffer.isEmpty())
    {
        proto::desktop::Config config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopManageConfig();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setDesktopManageConfig(const proto::desktop::Config& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    settings_.setValue(kDesktopManageConfigParam, buffer);
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Config ClientSettings::desktopViewConfig() const
{
    QByteArray buffer = settings_.value(kDesktopViewConfigParam).toByteArray();
    if (!buffer.isEmpty())
    {
        proto::desktop::Config config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopViewConfig();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setDesktopViewConfig(const proto::desktop::Config& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    settings_.setValue(kDesktopViewConfigParam, buffer);
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
