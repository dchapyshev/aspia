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

#include "client/ui/client_settings.h"

#include "client/config_factory.h"

#include <QLocale>

namespace client {

ClientSettings::ClientSettings()
    : settings_(QSettings::IniFormat, QSettings::UserScope, "aspia", "client")
{
    // Nothing
}

QString ClientSettings::locale() const
{
    return settings_.value(QLatin1String("Locale"), QLocale::system().bcp47Name()).toString();
}

void ClientSettings::setLocale(const QString& locale)
{
    settings_.setValue(QLatin1String("Locale"), locale);
}

QStringList ClientSettings::addressList() const
{
    return settings_.value("AddressList").toStringList();
}

void ClientSettings::setAddressList(const QStringList& list)
{
    settings_.setValue("AddressList", list);
}

proto::SessionType ClientSettings::sessionType() const
{
    return static_cast<proto::SessionType>(
        settings_.value("SessionType", proto::SESSION_TYPE_DESKTOP_MANAGE).toUInt());
}

void ClientSettings::setSessionType(proto::SessionType session_type)
{
    settings_.setValue("SessionType", static_cast<uint32_t>(session_type));
}

proto::DesktopConfig ClientSettings::desktopManageConfig() const
{
    QByteArray buffer = settings_.value("DesktopManageConfig").toByteArray();
    if (!buffer.isEmpty())
    {
        proto::DesktopConfig config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopManageConfig();
}

void ClientSettings::setDesktopManageConfig(const proto::DesktopConfig& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer.data()));
    settings_.setValue("DesktopManageConfig", buffer);
}

proto::DesktopConfig ClientSettings::desktopViewConfig() const
{
    QByteArray buffer = settings_.value("DesktopViewConfig").toByteArray();
    if (!buffer.isEmpty())
    {
        proto::DesktopConfig config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopViewConfig();
}

void ClientSettings::setDesktopViewConfig(const proto::DesktopConfig& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer.data()));
    settings_.setValue("DesktopViewConfig", buffer);
}

} // namespace client
