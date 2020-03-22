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
#include "proxy/session_key.h"

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

    explicit Controller(std::unique_ptr<net::Channel> channel, Delegate* delegate);
    ~Controller();

    void start();

protected:
    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    std::unique_ptr<net::Channel> channel_;

    proto::RouterToProxy incoming_message_;
    proto::ProxyToRouter outgoing_message_;

    std::map<uint32_t, SessionKey> pool_;
    uint32_t current_key_id_ = 0;

    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(Controller);
};

} // namespace proxy

#endif // PROXY__CONTROLLER_H
