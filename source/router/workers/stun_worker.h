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

#ifndef ROUTER_WORKERS_STUN_WORKER_H
#define ROUTER_WORKERS_STUN_WORKER_H

#include <asio/ip/udp.hpp>

#include <array>
#include <memory>

#include "base/shared_pointer.h"
#include "base/threading/worker.h"

class StunWorker final : public Worker
{
    Q_OBJECT

public:
    StunWorker();
    ~StunWorker() final;

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    bool startServer(quint16 port);
    void doReceiveRequest();
    bool doSendAddressReply(quint32 transaction_id, const asio::ip::udp::endpoint& remote_endpoint);

    struct IoState
    {
        bool alive = true;
        asio::ip::udp::endpoint remote_endpoint;
        std::array<quint8, 1024> read_buffer;
    };

    SharedPointer<IoState> io_;

    // Created in the worker thread so the socket binds to its io_context.
    std::unique_ptr<asio::ip::udp::socket> udp_socket_;

    Q_DISABLE_COPY_MOVE(StunWorker)
};

#endif // ROUTER_WORKERS_STUN_WORKER_H
