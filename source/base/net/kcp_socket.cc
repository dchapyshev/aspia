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

#include "base/net/kcp_socket.h"

#include <QTimer>

#include <ikcp.h>

#include "base/logging.h"

namespace base {

namespace {

const quint32 kKcpConv = 1;
const int kKcpUpdateIntervalMs = 10;
const int kKcpMtu = 1200;
const int kMaxKcpChunk = 120 * (kKcpMtu - 24);
const int kUdpSendQueueReservedSize = 256;

//--------------------------------------------------------------------------------------------------
quint32 currentTimeMs()
{
    return static_cast<quint32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() & 0xFFFFFFFF);
}

} // namespace

//--------------------------------------------------------------------------------------------------
KcpSocket::KcpSocket(asio::ip::udp::socket& socket, QObject* parent)
    : QObject(parent),
      socket_(socket)
{
    udp_send_queue_.reserve(kUdpSendQueueReservedSize);

    update_timer_ = new QTimer(this);
    update_timer_->setTimerType(Qt::PreciseTimer);
    connect(update_timer_, &QTimer::timeout, this, &KcpSocket::doKcpUpdate);

    kcp_ = ikcp_create(kKcpConv, this);
    if (!kcp_)
    {
        LOG(ERROR) << "Failed to create KCP object";
        return;
    }

    kcp_->output = kcpOutputCallback;
    kcp_->stream = 1;

    // Set KCP to fast mode: nodelay=1, interval=10ms, fast resend=2, no congestion control.
    ikcp_nodelay(kcp_, 1, kKcpUpdateIntervalMs, 2, 1);
    ikcp_setmtu(kcp_, kKcpMtu);
    ikcp_wndsize(kcp_, 128, 128);
}

//--------------------------------------------------------------------------------------------------
KcpSocket::~KcpSocket()
{
    update_timer_->stop();

    if (kcp_)
    {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::start()
{
    update_timer_->start(kKcpUpdateIntervalMs);
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::stop()
{
    update_timer_->stop();
}

//--------------------------------------------------------------------------------------------------
bool KcpSocket::send(const char* data, int size)
{
    if (!kcp_)
        return false;

    const char* ptr = data;
    int remaining = size;

    while (remaining > 0)
    {
        int chunk = std::min(remaining, kMaxKcpChunk);
        int ret = ikcp_send(kcp_, ptr, chunk);
        if (ret < 0)
        {
            LOG(ERROR) << "ikcp_send failed with code:" << ret;
            emit sig_errorOccurred();
            return false;
        }
        ptr += chunk;
        remaining -= chunk;
    }

    // Flush KCP to send all queued segments immediately.
    ikcp_flush(kcp_);
    return true;
}

//--------------------------------------------------------------------------------------------------
int KcpSocket::input(const char* data, int size)
{
    if (!kcp_)
        return -1;

    return ikcp_input(kcp_, data, static_cast<long>(size));
}

//--------------------------------------------------------------------------------------------------
int KcpSocket::peekSize()
{
    if (!kcp_)
        return -1;

    return ikcp_peeksize(kcp_);
}

//--------------------------------------------------------------------------------------------------
int KcpSocket::recv(char* buf, int size)
{
    if (!kcp_)
        return -1;

    return ikcp_recv(kcp_, buf, size);
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::setConnected(bool connected)
{
    connected_ = connected;
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::flushUdpQueue()
{
    if (!udp_sending_ && !udp_send_queue_.isEmpty())
        doUdpSend();
}

//--------------------------------------------------------------------------------------------------
// static
int KcpSocket::kcpOutputCallback(const char* buf, int len, IKCPCB* /* kcp */, void* user)
{
    KcpSocket* self = static_cast<KcpSocket*>(user);
    if (!self || !self->socket_.is_open())
        return -1;

    self->udp_send_queue_.emplace_back(buf, len);

    if (!self->udp_sending_)
        self->doUdpSend();

    return 0;
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::doUdpSend()
{
    if (udp_send_queue_.isEmpty())
    {
        udp_sending_ = false;
        return;
    }

    if (!connected_)
    {
        // Can't send until we know the peer's address (learned on first receive).
        // Keep packets in the queue; they will be sent once the peer connects.
        udp_sending_ = false;
        return;
    }

    udp_sending_ = true;

    QByteArray data = std::move(udp_send_queue_.front());
    udp_send_queue_.pop_front();

    socket_.async_send(asio::buffer(data.constData(), data.size()),
        [this, data](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
            {
                LOG(ERROR) << "UDP send error:" << error_code;
                emit sig_errorOccurred();
            }
            return;
        }

        doUdpSend();
    });
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::doKcpUpdate()
{
    if (!kcp_)
        return;

    ikcp_update(kcp_, currentTimeMs());
}

} // namespace base
