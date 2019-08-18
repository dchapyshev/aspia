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

#ifndef CLIENT__CLIENT_AUTHENTICATOR_H
#define CLIENT__CLIENT_AUTHENTICATOR_H

#include "base/macros_magic.h"
#include "base/version.h"
#include "net/network_listener.h"
#include "proto/key_exchange.pb.h"

#include <functional>

namespace crypto {
class Cryptor;
} // namespace crypto

namespace net {
class ChannelProxy;
} // namespace net

namespace client {

class Authenticator : public net::Listener
{
public:
    Authenticator() = default;
    virtual ~Authenticator() = default;

    void setSessionType(proto::SessionType session_type);
    proto::SessionType sessionType() const { return session_type_; }

    proto::Method method() const { return method_; }
    const base::Version& peerVersion() const { return peer_version_; }

    enum class Result
    {
        SUCCESS,
        UNKNOWN_ERROR,
        NETWORK_ERROR,
        PROTOCOL_ERROR,
        ACCESS_DENIED,
        SESSION_DENIED
    };

    using Callback = std::function<void(Result result)>;

    void start(std::shared_ptr<net::ChannelProxy> channel, Callback callback);

protected:
    virtual uint32_t methods() const = 0;
    virtual void onStarted() = 0;
    virtual bool onMessage(const base::ByteArray& message) = 0;
    virtual std::unique_ptr<crypto::Cryptor> takeCryptor() = 0;

    void sendMessage(base::ByteArray&& message);
    void onFinished(Result result);

    // net::Listener implementation.
    void onNetworkConnected() override;
    void onNetworkDisconnected() override;
    void onNetworkError(net::ErrorCode error_code) override;
    void onNetworkMessage(base::ByteArray& buffer) override;

private:
    enum class State
    {
        HELLO,
        AUTHENTICATION,
        SESSION
    };

    std::shared_ptr<net::ChannelProxy> channel_;
    Callback callback_;

    State state_ = State::HELLO;
    proto::Method method_ = proto::METHOD_UNKNOWN;
    proto::SessionType session_type_ = proto::SESSION_TYPE_UNKNOWN;
    base::Version peer_version_;

    DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

} // namespace client

#endif // CLIENT__CLIENT_AUTHENTICATOR_H
