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

#include "base/net/enet_util.h"

#include <enet/enet.h>

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedENetHost::ScopedENetHost(ENetHost* host)
    : host_(host)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ScopedENetHost::ScopedENetHost(ScopedENetHost&& other) noexcept
    : host_(other.host_)
{
    other.host_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
ScopedENetHost& ScopedENetHost::operator=(ScopedENetHost&& other) noexcept
{
    if (this != &other)
    {
        reset();
        host_ = other.host_;
        other.host_ = nullptr;
    }

    return *this;
}

//--------------------------------------------------------------------------------------------------
ScopedENetHost::~ScopedENetHost()
{
    reset();
}

//--------------------------------------------------------------------------------------------------
ENetHost* ScopedENetHost::get() const noexcept
{
    return host_;
}

//--------------------------------------------------------------------------------------------------
void ScopedENetHost::reset(ENetHost* host) noexcept
{
    if (host_)
        enet_host_destroy(host_);
    host_ = host;
}

//--------------------------------------------------------------------------------------------------
ENetHost* ScopedENetHost::release() noexcept
{
    ENetHost* tmp = host_;
    host_ = nullptr;
    return tmp;
}

//--------------------------------------------------------------------------------------------------
ENetHost* ScopedENetHost::operator->() const noexcept
{
    return host_;
}

//--------------------------------------------------------------------------------------------------
ScopedENetHost::operator bool() const noexcept
{
    return host_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
ScopedENetHost::operator ENetHost*() noexcept
{
    return host_;
}

//--------------------------------------------------------------------------------------------------
ScopedENetPeer::ScopedENetPeer(ENetPeer* peer)
    : peer_(peer)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ScopedENetPeer::ScopedENetPeer(ScopedENetPeer&& other) noexcept
    : peer_(other.peer_)
{
    other.peer_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
ScopedENetPeer& ScopedENetPeer::operator=(ScopedENetPeer&& other) noexcept
{
    if (this != &other)
    {
        reset();
        peer_ = other.peer_;
        other.peer_ = nullptr;
    }

    return *this;
}

//--------------------------------------------------------------------------------------------------
ScopedENetPeer::~ScopedENetPeer()
{
    reset();
}

//--------------------------------------------------------------------------------------------------
ENetPeer* ScopedENetPeer::get() const noexcept
{
    return peer_;
}

//--------------------------------------------------------------------------------------------------
void ScopedENetPeer::reset(ENetPeer* peer) noexcept
{
    if (peer_)
        enet_peer_reset(peer_);
    peer_ = peer;
}

//--------------------------------------------------------------------------------------------------
ENetPeer* ScopedENetPeer::release() noexcept
{
    ENetPeer* tmp = peer_;
    peer_ = nullptr;
    return tmp;
}

//--------------------------------------------------------------------------------------------------
ENetPeer* ScopedENetPeer::operator->() const noexcept
{
    return peer_;
}

//--------------------------------------------------------------------------------------------------
ScopedENetPeer::operator bool() const noexcept
{
    return peer_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
ScopedENetPeer::operator ENetPeer*() noexcept
{
    return peer_;
}

} // namespace base
