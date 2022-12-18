//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/files/base_paths.h"
#include "build/build_config.h"

namespace relay {

namespace {

const base::JsonSettings::Scope kScope = base::JsonSettings::Scope::SYSTEM;
const char kApplicationName[] = "aspia";
const char kFileName[] = "relay";

} // namespace

Settings::Settings()
    : impl_(kScope, kApplicationName, kFileName)
{
    // Nothing
}

Settings::~Settings() = default;

// static
std::filesystem::path Settings::filePath()
{
    return base::JsonSettings::filePath(kScope, kApplicationName, kFileName);
}

void Settings::reset()
{
    setRouterAddress(u"localhost");
    setRouterPort(DEFAULT_ROUTER_TCP_PORT);
    setRouterPublicKey(base::ByteArray());
    setPeerAddress(std::u16string());
    setPeerPort(DEFAULT_RELAY_PEER_TCP_PORT);
    setPeerIdleTimeout(std::chrono::minutes(5));
    setMaxPeerCount(100);
    setStatisticsEnabled(false);
    setStatisticsInterval(std::chrono::seconds(5));
}

void Settings::flush()
{
    impl_.flush();
}

void Settings::setRouterAddress(const std::u16string& address)
{
    impl_.set<std::u16string>("RouterAddress", address);
}

std::u16string Settings::routerAddress() const
{
    return impl_.get<std::u16string>("RouterAddress");
}

void Settings::setRouterPort(uint16_t port)
{
    impl_.set<uint16_t>("RouterPort", port);
}

uint16_t Settings::routerPort() const
{
    return impl_.get<uint16_t>("RouterPort", DEFAULT_ROUTER_TCP_PORT);
}

void Settings::setRouterPublicKey(const base::ByteArray& public_key)
{
    impl_.set<base::ByteArray>("RouterPublicKey", public_key);
}

base::ByteArray Settings::routerPublicKey() const
{
    return impl_.get<base::ByteArray>("RouterPublicKey");
}

void Settings::setListenInterface(const std::u16string& interface)
{
    impl_.set<std::u16string>("ListenInterface", interface);
}

std::u16string Settings::listenInterface() const
{
    std::u16string interface = impl_.get<std::u16string>("ListenInterface", u"0.0.0.0");
    if (interface.empty())
        return u"0.0.0.0";

    return interface;
}

void Settings::setPeerAddress(const std::u16string& address)
{
    impl_.set<std::u16string>("PeerAddress", address);
}

std::u16string Settings::peerAddress() const
{
    return impl_.get<std::u16string>("PeerAddress");
}

void Settings::setPeerPort(uint16_t port)
{
    impl_.set<uint16_t>("PeerPort", port);
}

uint16_t Settings::peerPort() const
{
    return impl_.get<uint16_t>("PeerPort", DEFAULT_RELAY_PEER_TCP_PORT);
}

void Settings::setPeerIdleTimeout(const std::chrono::minutes& timeout)
{
    impl_.set<int>("PeerIdleTimeout", timeout.count());
}

std::chrono::minutes Settings::peerIdleTimeout() const
{
    return std::chrono::minutes(impl_.get<int>("PeerIdleTimeout", 5));
}

void Settings::setMaxPeerCount(uint32_t count)
{
    impl_.set<uint32_t>("MaxPeerCount", count);
}

uint32_t Settings::maxPeerCount() const
{
    return impl_.get<uint32_t>("MaxPeerCount", 100);
}

void Settings::setStatisticsEnabled(bool enable)
{
    impl_.set<bool>("StatisticsEnabled", enable);
}

bool Settings::isStatisticsEnabled() const
{
    return impl_.get<bool>("StatisticsEnabled", false);
}

void Settings::setStatisticsInterval(const std::chrono::seconds& interval)
{
    impl_.set<int>("StatisticsInterval", static_cast<int>(interval.count()));
}

std::chrono::seconds Settings::statisticsInterval() const
{
    return std::chrono::seconds(impl_.get<int>("StatisticsInterval", 5));
}

} // namespace relay
