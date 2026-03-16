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

#ifndef BASE_PEER_STUN_PEER_H
#define BASE_PEER_STUN_PEER_H

#include <QObject>

#include <asio/ip/udp.hpp>

#include "base/shared_pointer.h"

class QTimer;

namespace base {

class Location;
class UdpChannel;

class StunPeer final : public QObject
{
    Q_OBJECT

public:
    explicit StunPeer(QObject* parent = nullptr);
    ~StunPeer();

    void start(const QString& stun_host, quint16 stun_port);
    UdpChannel* takeChannel();

signals:
    void sig_channelReady(const QString& external_address, quint16 external_port);
    void sig_errorOccurred();

private slots:
    void onAttempt();

private:
    void doStart();
    void doStop();
    void doReceiveExternalAddress();
    void onErrorOccurred(const Location& location, const std::error_code& error_code);

    QTimer* timer_ = nullptr;
    int number_of_attempts_ = 0;

    std::string stun_host_;
    std::string stun_port_;

    SharedPointer<bool> alive_guard_ { new bool(true) };
    asio::ip::udp::resolver udp_resolver_;
    asio::ip::udp::socket udp_socket_;
    std::array<quint8, 1024> buffer_;

    UdpChannel* ready_channel_ = nullptr;

    Q_DISABLE_COPY_MOVE(StunPeer)
};

} // namespace base

#endif // BASE_PEER_STUN_PEER_H
