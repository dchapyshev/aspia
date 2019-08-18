//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__CLIENT_SESSION_H
#define HOST__CLIENT_SESSION_H

#include "base/version.h"
#include "net/network_listener.h"
#include "proto/common.pb.h"

namespace net {
class Channel;
class ChannelProxy;
} // namespace net

namespace host {

class ClientSession : public net::Listener
{
public:
    virtual ~ClientSession() = default;

    static std::unique_ptr<ClientSession> create(
        proto::SessionType session_type, std::unique_ptr<net::Channel> channel);

    void start();

    std::string id() const { return id_; }

    void setVersion(const base::Version& version);
    const base::Version& version() const { return version_; }

    void setUserName(std::u16string_view username);
    const std::u16string& userName() const { return username_; }

    proto::SessionType sessionType() const { return session_type_; }
    std::u16string peerAddress() const;

protected:
    ClientSession(proto::SessionType session_type, std::unique_ptr<net::Channel> channel);

    void sendMessage(base::ByteArray&& buffer);

    // net::Listener implementation.
    void onNetworkConnected() override;
    void onNetworkDisconnected() override;
    void onNetworkError(net::ErrorCode error_code) override;

private:
    std::string id_;
    proto::SessionType session_type_;
    base::Version version_;
    std::u16string username_;

    std::unique_ptr<net::Channel> channel_;
    std::shared_ptr<net::ChannelProxy> channel_proxy_;
};

} // namespace host

#endif // HOST__CLIENT_SESSION_H
