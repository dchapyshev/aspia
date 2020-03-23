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

#ifndef PROXY__CONTROLLER_H
#define PROXY__CONTROLLER_H

#include "base/macros_magic.h"
#include "net/network_listener.h"
#include "proto/proxy.pb.h"
#include "proxy/shared_pool.h"

namespace net {
class Channel;
} // namespace net

namespace proxy {

class Controller : public net::Listener
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onControllerFinished(Controller* controller) = 0;
    };

    Controller(uint32_t controller_id,
               std::unique_ptr<SharedPool> shared_pool,
               std::unique_ptr<net::Channel> channel,
               Delegate* delegate);
    ~Controller();

    void start();
    void stop();

    uint32_t id() const { return controller_id_; }

protected:
    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    const uint32_t controller_id_;

    std::unique_ptr<SharedPool> shared_pool_;
    std::unique_ptr<net::Channel> channel_;

    proto::RouterToProxy incoming_message_;
    proto::ProxyToRouter outgoing_message_;

    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(Controller);
};

} // namespace proxy

#endif // PROXY__CONTROLLER_H
