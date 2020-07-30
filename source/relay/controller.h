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

#ifndef RELAY__CONTROLLER_H
#define RELAY__CONTROLLER_H

#include "base/net/network_channel.h"
#include "proto/proxy.pb.h"
#include "relay/shared_pool.h"

namespace relay {

class Controller : public base::NetworkChannel::Listener
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
               std::unique_ptr<base::NetworkChannel> channel,
               Delegate* delegate);
    ~Controller();

    void start();
    void stop();

    uint32_t id() const { return controller_id_; }

protected:
    // base::NetworkChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    const uint32_t controller_id_;

    std::unique_ptr<SharedPool> shared_pool_;
    std::unique_ptr<base::NetworkChannel> channel_;

    proto::RouterToProxy incoming_message_;
    proto::ProxyToRouter outgoing_message_;

    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(Controller);
};

} // namespace relay

#endif // RELAY__CONTROLLER_H
