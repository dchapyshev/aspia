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

#ifndef BASE_NET_KCP_SOCKET_H
#define BASE_NET_KCP_SOCKET_H

#include <QObject>
#include <QQueue>

#include <array>

#include <asio/ip/udp.hpp>

#include "base/shared_pointer.h"

struct IKCPCB;

namespace base {

class KcpSocket final : public QObject
{
    Q_OBJECT

public:
    explicit KcpSocket(QObject* parent = nullptr);
    KcpSocket(asio::ip::udp::socket&& socket, QObject* parent = nullptr);
    ~KcpSocket() final;

    static KcpSocket* bind(quint16& port);

    void connectTo(const QString& address, quint16 port);
    bool send(const void* data, int size);
    void close();

signals:
    void sig_connected();
    void sig_dataReceived(const char* data, int size); // Only direct connection.
    void sig_errorOccurred();

protected:
    void timerEvent(QTimerEvent* event) final;

private:
    void init();

    static int kcpOutputCallback(const char* buf, int len, IKCPCB* kcp, void* user);

    void doRead();
    void onUdpDataReceived(size_t bytes_transferred);
    void doUdpSend();
    void sendHandshake();

    SharedPointer<bool> alive_guard_ { new bool(true) };
    asio::ip::udp::resolver resolver_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
    IKCPCB* kcp_ = nullptr;
    int update_timer_id_ = 0;

    QQueue<QByteArray> udp_send_queue_;
    QByteArray udp_send_active_;
    bool udp_sending_ = false;
    bool has_remote_endpoint_ = false;
    bool connected_ = false;
    bool reading_ = false;

    QByteArray kcp_read_buffer_;

    static const int kRecvBufferSize = 65536;
    std::array<char, kRecvBufferSize> recv_buffer_;

    Q_DISABLE_COPY_MOVE(KcpSocket)
};

} // namespace base

#endif // BASE_NET_KCP_SOCKET_H
