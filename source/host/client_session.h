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

#ifndef HOST__CLIENT_SESSION_H
#define HOST__CLIENT_SESSION_H

#include "base/session_id.h"
#include "base/version.h"
#include "base/net/network_channel.h"
#include "proto/common.pb.h"

namespace base {
class NetworkChannelProxy;
} // namespace base

namespace host {

class DesktopSessionProxy;

class ClientSession : public base::NetworkChannel::Listener
{
public:
    virtual ~ClientSession() = default;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onClientSessionConfigured() = 0;
        virtual void onClientSessionFinished() = 0;
    };

    enum class State
    {
        CREATED, // Session created but not yet started.
        STARTED, // Session started.
        FINISHED // Session is stopped.
    };

    static std::unique_ptr<ClientSession> create(
        proto::SessionType session_type, std::unique_ptr<base::NetworkChannel> channel);

    void start(Delegate* delegate);
    void stop();

    State state() const { return state_; }
    std::string id() const { return id_; }

    void setVersion(const base::Version& version);
    const base::Version& version() const { return version_; }

    void setUserName(std::string_view username);
    const std::string& userName() const { return username_; }

    proto::SessionType sessionType() const { return session_type_; }
    std::u16string peerAddress() const;

    void setSessionId(base::SessionId session_id);
    base::SessionId sessionId() const { return session_id_; }

protected:
    ClientSession(proto::SessionType session_type, std::unique_ptr<base::NetworkChannel> channel);

    virtual void onStarted() = 0;
    std::shared_ptr<base::NetworkChannelProxy> channelProxy();
    void sendMessage(base::ByteArray&& buffer);

    // base::NetworkChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;

    Delegate* delegate_ = nullptr;

private:
    base::SessionId session_id_ = base::kInvalidSessionId;
    State state_ = State::CREATED;
    std::string id_;
    proto::SessionType session_type_;
    base::Version version_;
    std::string username_;

    std::unique_ptr<base::NetworkChannel> channel_;
};

} // namespace host

#endif // HOST__CLIENT_SESSION_H
