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

#ifndef BASE_PEER_STUN_SERVER_H
#define BASE_PEER_STUN_SERVER_H

#include <QObject>

#include <array>

#include <asio/ip/udp.hpp>

namespace base {

class StunServer final : public QObject
{
    Q_OBJECT

public:
    explicit StunServer(QObject* parent = nullptr);
    ~StunServer();

    void start(quint16 port);

private:
    void doReceiveRequest();
    bool doSendAddressReply();

    asio::ip::udp::socket udp_socket_;
    std::array<quint8, 1024> buffer_;

    Q_DISABLE_COPY_MOVE(StunServer)
};

} // namespace base

#endif // BASE_PEER_STUN_SERVER_H
