//
// SmartCafe Project
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

#include "relay/settings.h"

#include "base/xml_settings.h"
#include "build/build_config.h"

namespace relay {

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    : impl_(base::XmlSettings::format(), QSettings::SystemScope, "aspia", "relay")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Settings::~Settings() = default;

//--------------------------------------------------------------------------------------------------
QString Settings::filePath()
{
    return impl_.fileName();
}

//--------------------------------------------------------------------------------------------------
bool Settings::isEmpty() const
{
    return impl_.allKeys().isEmpty();
}

//--------------------------------------------------------------------------------------------------
void Settings::reset()
{
    setRouterAddress("127.0.0.1");
    setRouterPort(DEFAULT_ROUTER_TCP_PORT);
    setRouterPublicKey(QByteArray());
    setPeerAddress(QString());
    setPeerPort(DEFAULT_RELAY_PEER_TCP_PORT);
    setPeerIdleTimeout(std::chrono::minutes(5));
    setMaxPeerCount(100);
    setStatisticsEnabled(false);
    setStatisticsInterval(std::chrono::seconds(5));
}

//--------------------------------------------------------------------------------------------------
void Settings::sync()
{
    impl_.sync();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterAddress(const QString& address)
{
    impl_.setValue("router_address", address);
}

//--------------------------------------------------------------------------------------------------
QString Settings::routerAddress() const
{
    return impl_.value("router_address").toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterPort(quint16 port)
{
    impl_.setValue("router_port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::routerPort() const
{
    return impl_.value("router_port", DEFAULT_ROUTER_TCP_PORT).toUInt();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterPublicKey(const QByteArray& public_key)
{
    impl_.setValue("router_public_key", public_key);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::routerPublicKey() const
{
    return impl_.value("router_public_key").toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setListenInterface(const QString& iface)
{
    impl_.setValue("listen_interface", iface);
}

//--------------------------------------------------------------------------------------------------
QString Settings::listenInterface() const
{
    return impl_.value("listen_interface").toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerAddress(const QString& address)
{
    impl_.setValue("peer_address", address);
}

//--------------------------------------------------------------------------------------------------
QString Settings::peerAddress() const
{
    return impl_.value("peer_address").toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerPort(quint16 port)
{
    impl_.setValue("peer_port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::peerPort() const
{
    return impl_.value("peer_port", DEFAULT_RELAY_PEER_TCP_PORT).toUInt();
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerIdleTimeout(const std::chrono::minutes& timeout)
{
    impl_.setValue("peer_idle_timeout", static_cast<qint64>(timeout.count()));
}

//--------------------------------------------------------------------------------------------------
std::chrono::minutes Settings::peerIdleTimeout() const
{
    return std::chrono::minutes(impl_.value("peer_idle_timeout", 5).toInt());
}

//--------------------------------------------------------------------------------------------------
void Settings::setMaxPeerCount(quint32 count)
{
    impl_.setValue("max_peer_count", count);
}

//--------------------------------------------------------------------------------------------------
quint32 Settings::maxPeerCount() const
{
    return impl_.value("max_peer_count", 100).toUInt();
}

//--------------------------------------------------------------------------------------------------
void Settings::setStatisticsEnabled(bool enable)
{
    impl_.setValue("statistics_enabled", enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isStatisticsEnabled() const
{
    return impl_.value("statistics_enabled", false).toBool();
}

//--------------------------------------------------------------------------------------------------
void Settings::setStatisticsInterval(const std::chrono::seconds& interval)
{
    impl_.setValue("statistics_interval", static_cast<int>(interval.count()));
}

//--------------------------------------------------------------------------------------------------
std::chrono::seconds Settings::statisticsInterval() const
{
    return std::chrono::seconds(impl_.value("statistics_interval", 5).toInt());
}

} // namespace relay
