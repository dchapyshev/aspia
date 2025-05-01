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

#include "relay/settings.h"

#include "build/build_config.h"

namespace relay {

namespace {

const base::JsonSettings::Scope kScope = base::JsonSettings::Scope::SYSTEM;
const char kApplicationName[] = "aspia";
const char kFileName[] = "relay";

} // namespace

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    : impl_(kScope, kApplicationName, kFileName)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Settings::~Settings() = default;

//--------------------------------------------------------------------------------------------------
// static
std::filesystem::path Settings::filePath()
{
    return base::JsonSettings::filePath(kScope, kApplicationName, kFileName);
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
void Settings::flush()
{
    impl_.flush();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterAddress(const QString& address)
{
    impl_.set<QString>("RouterAddress", address);
}

//--------------------------------------------------------------------------------------------------
QString Settings::routerAddress() const
{
    return impl_.get<QString>("RouterAddress");
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterPort(quint16 port)
{
    impl_.set<quint16>("RouterPort", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::routerPort() const
{
    return impl_.get<quint16>("RouterPort", DEFAULT_ROUTER_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterPublicKey(const QByteArray& public_key)
{
    impl_.set<QByteArray>("RouterPublicKey", public_key);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::routerPublicKey() const
{
    return impl_.get<QByteArray>("RouterPublicKey");
}

//--------------------------------------------------------------------------------------------------
void Settings::setListenInterface(const QString& interface)
{
    impl_.set<QString>("ListenInterface", interface);
}

//--------------------------------------------------------------------------------------------------
QString Settings::listenInterface() const
{
    return impl_.get<QString>("ListenInterface", QString());
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerAddress(const QString& address)
{
    impl_.set<QString>("PeerAddress", address);
}

//--------------------------------------------------------------------------------------------------
QString Settings::peerAddress() const
{
    return impl_.get<QString>("PeerAddress");
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerPort(quint16 port)
{
    impl_.set<quint16>("PeerPort", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::peerPort() const
{
    return impl_.get<quint16>("PeerPort", DEFAULT_RELAY_PEER_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerIdleTimeout(const std::chrono::minutes& timeout)
{
    impl_.set<int>("PeerIdleTimeout", timeout.count());
}

//--------------------------------------------------------------------------------------------------
std::chrono::minutes Settings::peerIdleTimeout() const
{
    return std::chrono::minutes(impl_.get<int>("PeerIdleTimeout", 5));
}

//--------------------------------------------------------------------------------------------------
void Settings::setMaxPeerCount(uint32_t count)
{
    impl_.set<uint32_t>("MaxPeerCount", count);
}

//--------------------------------------------------------------------------------------------------
uint32_t Settings::maxPeerCount() const
{
    return impl_.get<uint32_t>("MaxPeerCount", 100);
}

//--------------------------------------------------------------------------------------------------
void Settings::setStatisticsEnabled(bool enable)
{
    impl_.set<bool>("StatisticsEnabled", enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isStatisticsEnabled() const
{
    return impl_.get<bool>("StatisticsEnabled", false);
}

//--------------------------------------------------------------------------------------------------
void Settings::setStatisticsInterval(const std::chrono::seconds& interval)
{
    impl_.set<int>("StatisticsInterval", static_cast<int>(interval.count()));
}

//--------------------------------------------------------------------------------------------------
std::chrono::seconds Settings::statisticsInterval() const
{
    return std::chrono::seconds(impl_.get<int>("StatisticsInterval", 5));
}

} // namespace relay
