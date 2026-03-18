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

#ifndef BASE_NET_ENET_UTIL_H
#define BASE_NET_ENET_UTIL_H

#include <QtGlobal>

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace base {

class ScopedENetHost
{
public:
    ScopedENetHost() = default;
    explicit ScopedENetHost(ENetHost* host);

    ScopedENetHost(ScopedENetHost&& other) noexcept;
    ScopedENetHost& operator=(ScopedENetHost&& other) noexcept;

    ~ScopedENetHost();

    ENetHost* get() const noexcept;
    void reset(ENetHost* host = nullptr) noexcept;
    ENetHost* release() noexcept;

    ENetHost* operator->() const noexcept;
    explicit operator bool() const noexcept;
    operator ENetHost*() noexcept;

private:
    ENetHost* host_ = nullptr;
    Q_DISABLE_COPY(ScopedENetHost)
};

class ScopedENetPeer
{
public:
    ScopedENetPeer() = default;
    explicit ScopedENetPeer(ENetPeer* peer);

    ScopedENetPeer(ScopedENetPeer&& other) noexcept;
    ScopedENetPeer& operator=(ScopedENetPeer&& other) noexcept;

    ~ScopedENetPeer();

    ENetPeer* get() const noexcept;
    void reset(ENetPeer* peer = nullptr) noexcept;
    ENetPeer* release() noexcept;

    ENetPeer* operator->() const noexcept;
    explicit operator bool() const noexcept;
    operator ENetPeer*() noexcept;

private:
    ENetPeer* peer_ = nullptr;
    Q_DISABLE_COPY(ScopedENetPeer)
};

} // namespace base

#endif // BASE_NET_ENET_UTIL_H
