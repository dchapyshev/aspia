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

#include "base/net/enet_socket.h"

#include <QTimerEvent>

#include <enet/enet.h>

#include "base/logging.h"

namespace base {

namespace {

const int kUpdateIntervalMs = 10;
const int kMaxPeers = 1;
const int kChannelCount = 256;

//--------------------------------------------------------------------------------------------------
void ensureEnetInitialized()
{
    static struct Initializer
    {
        Initializer()
        {
            int ret = enet_initialize();
            CHECK(ret == 0) << "Failed to initialize ENet";
        }
        ~Initializer() { enet_deinitialize(); }
    } initializer;
}

} // namespace

//--------------------------------------------------------------------------------------------------
EnetSocket::EnetSocket(QObject* parent)
    : QObject(parent)
{
    ensureEnetInitialized();
}

//--------------------------------------------------------------------------------------------------
EnetSocket::~EnetSocket()
{
    close();
}

//--------------------------------------------------------------------------------------------------
// static
EnetSocket* EnetSocket::bind(quint16& port)
{
    ensureEnetInitialized();

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    ENetHost* host = enet_host_create(&address, kMaxPeers, kChannelCount, 0, 0);
    if (!host)
    {
        LOG(ERROR) << "Unable to create ENet host on port" << port;
        return nullptr;
    }

    if (!port)
    {
        // Retrieve the actual port assigned by the OS.
        ENetAddress bound_address;
        if (enet_socket_get_address(host->socket, &bound_address) != 0)
        {
            LOG(ERROR) << "Unable to get bound port";
            enet_host_destroy(host);
            return nullptr;
        }

        port = bound_address.port;
    }

    EnetSocket* socket = new EnetSocket(nullptr);
    socket->init(host);
    return socket;
}

//--------------------------------------------------------------------------------------------------
void EnetSocket::connectTo(const QString& address, quint16 port)
{
    if (!host_)
    {
        ENetHost* host = enet_host_create(NULL, kMaxPeers, kChannelCount, 0, 0);
        if (!host)
        {
            LOG(ERROR) << "Failed to create ENet host";
            emit sig_errorOccurred();
            return;
        }

        init(host);
    }

    ENetAddress enet_address;
    enet_address.port = port;

    if (enet_address_set_host(&enet_address, address.toLocal8Bit().constData()) != 0)
    {
        LOG(ERROR) << "Failed to resolve address:" << address;
        emit sig_errorOccurred();
        return;
    }

    peer_ = enet_host_connect(host_, &enet_address, kChannelCount, 0);
    if (!peer_)
    {
        LOG(ERROR) << "Failed to initiate ENet connection";
        emit sig_errorOccurred();
        return;
    }

    enet_host_flush(host_);
}

//--------------------------------------------------------------------------------------------------
bool EnetSocket::send(const void* data, int size)
{
    if (!peer_ || !ready_)
    {
        LOG(ERROR) << "Cannot send: not connected";
        return false;
    }

    ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    if (!packet)
    {
        LOG(ERROR) << "Failed to create ENet packet";
        emit sig_errorOccurred();
        return false;
    }

    if (enet_peer_send(peer_, 0, packet) != 0)
    {
        LOG(ERROR) << "enet_peer_send failed";
        enet_packet_destroy(packet);
        emit sig_errorOccurred();
        return false;
    }

    enet_host_flush(host_);
    return true;
}

//--------------------------------------------------------------------------------------------------
void EnetSocket::close()
{
    if (update_timer_id_ != 0)
    {
        killTimer(update_timer_id_);
        update_timer_id_ = 0;
    }

    if (peer_)
    {
        enet_peer_reset(peer_);
        peer_ = nullptr;
    }

    if (host_)
    {
        enet_host_destroy(host_);
        host_ = nullptr;
    }

    ready_ = false;
}

//--------------------------------------------------------------------------------------------------
void EnetSocket::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != update_timer_id_)
        return;

    processEvents();
}

//--------------------------------------------------------------------------------------------------
void EnetSocket::init(ENetHost* host)
{
    host_ = host;
    update_timer_id_ = startTimer(kUpdateIntervalMs, Qt::PreciseTimer);
}

//--------------------------------------------------------------------------------------------------
void EnetSocket::processEvents()
{
    if (!host_)
        return;

    ENetEvent event;
    int result;

    while ((result = enet_host_service(host_, &event, 0)) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                LOG(INFO) << "ENet peer connected";
                peer_ = event.peer;
                ready_ = true;
                emit sig_ready();
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                emit sig_dataReceived(
                    reinterpret_cast<const char*>(event.packet->data),
                    static_cast<int>(event.packet->dataLength));
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                LOG(INFO) << "ENet peer disconnected";
                peer_ = nullptr;
                ready_ = false;
                emit sig_errorOccurred();
                return; // Host may be destroyed by the slot, stop processing.

            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }

    if (result < 0)
    {
        LOG(ERROR) << "enet_host_service failed";
        emit sig_errorOccurred();
    }
}

} // namespace base
