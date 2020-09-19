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

#include "relay/settings.h"

#include "base/files/base_paths.h"
#include "build/build_config.h"

namespace relay {

Settings::Settings()
    : impl_("aspia_relay")
{
    // Nothing
}

Settings::~Settings() = default;

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

void Settings::setMaxPeerCount(uint32_t count)
{
    impl_.set<uint32_t>("MaxPeerCount", count);
}

uint32_t Settings::maxPeerCount() const
{
    return impl_.get<uint32_t>("MaxPeerCount", 100);
}

void Settings::setLogPath(const std::filesystem::path& path)
{
    impl_.set<std::filesystem::path>("LogPath", path);
}

std::filesystem::path Settings::logPath() const
{
    std::filesystem::path path = impl_.get<std::filesystem::path>("LogPath");
    if (path.empty())
    {
#if defined(OS_WIN)
        if (!base::BasePaths::commonAppData(&path))
            return std::filesystem::path();
        path.append("Aspia/Logs");
#endif
    }

    return path;
}

void Settings::setMinLogLevel(int level)
{
    impl_.set<int>("MinLogLevel", level);
}

int Settings::minLogLevel() const
{
    return impl_.get<int>("MinLogLevel", 1);
}

void Settings::setMaxLogAge(int age)
{
    impl_.set<int>("MaxLogAge", age);
}

int Settings::maxLogAge() const
{
    return impl_.get<int>("MaxLogAge", 7);
}

} // namespace relay
