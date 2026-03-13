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

#ifndef BASE_NET_UDP_CHANNEL_H
#define BASE_NET_UDP_CHANNEL_H

#include <QObject>
#include <QQueue>

#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>

struct IKCPCB;

class QTimer;

namespace base {

class Location;
class StunPeer;

class UdpChannel final : public QObject
{
    Q_OBJECT

public:
    explicit UdpChannel(QObject* parent = nullptr);
    ~UdpChannel() final;

    void connectTo(const QString& address, quint16 port);
    void send(quint8 channel_id, const QByteArray& buffer);

signals:
    void sig_connected();
    void sig_errorOccurred();
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);

protected:
    friend class StunPeer;
    UdpChannel(asio::ip::udp::socket&& socket, QObject* parent);

private:
    static int kcpOutputCallback(const char* buf, int len, IKCPCB* kcp, void* user);

    void initKcp();
    void onConnected();
    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void doRead();
    void doKcpUpdate();
    void processKcpRecv();

    asio::ip::udp::resolver resolver_;
    asio::ip::udp::socket socket_;

    IKCPCB* kcp_ = nullptr;
    QTimer* update_timer_ = nullptr;

    static const int kRecvBufferSize = 65536;
    std::array<uint8_t, kRecvBufferSize> recv_buffer_;

    Q_DISABLE_COPY_MOVE(UdpChannel)
};

} // namespace base

#endif // BASE_NET_UDP_CHANNEL_H
