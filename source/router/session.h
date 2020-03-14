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

#ifndef ROUTER__SESSION_H
#define ROUTER__SESSION_H

#include "base/macros_magic.h"
#include "net/network_listener.h"

#include <memory>
#include <string>

namespace net {
class Channel;
} // namespace net

namespace router {

class Session : public net::Listener
{
public:
    explicit Session(std::unique_ptr<net::Channel> channel);
    virtual ~Session();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onPeerFinished() = 0;
    };

    void start(Delegate* delegate);

    const std::string& peerId() const { return peer_id_; }

protected:
    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    std::unique_ptr<net::Channel> channel_;
    std::string peer_id_;

    Delegate* delegate_ = nullptr;
};

} // namespace router

#endif // ROUTER__SESSION_H
