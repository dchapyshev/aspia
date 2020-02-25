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

#ifndef CLIENT__CLIENT_AUTHENTICATOR_H
#define CLIENT__CLIENT_AUTHENTICATOR_H

#include "base/macros_magic.h"
#include "base/version.h"
#include "crypto/big_num.h"
#include "net/network_listener.h"
#include "proto/key_exchange.pb.h"

#include <functional>

namespace base {
class Location;
} // namespace base

namespace crypto {
class MessageDecryptor;
class MessageEncryptor;
} // namespace crypto

namespace net {
class Channel;
} // namespace net

namespace client {

class Authenticator : public net::Listener
{
public:
    Authenticator();
    ~Authenticator();

    void setUserName(std::u16string_view username);
    const std::u16string& userName() const;

    void setPassword(std::u16string_view password);
    const std::u16string& password() const;

    void setSessionType(proto::SessionType session_type);
    proto::SessionType sessionType() const { return session_type_; }

    proto::Method method() const { return method_; }
    const base::Version& peerVersion() const { return peer_version_; }

    enum class ErrorCode
    {
        SUCCESS,
        UNKNOWN_ERROR,
        NETWORK_ERROR,
        PROTOCOL_ERROR,
        ACCESS_DENIED,
        SESSION_DENIED
    };

    using Callback = std::function<void(ErrorCode error_code)>;

    // Starts authentication.
    // |callback| is called upon completion. The authenticator guarantees that no code inside it
    // will be executed after call callback (you can remove the authenticator inside this callback).
    void start(std::unique_ptr<net::Channel> channel, Callback callback);

    std::unique_ptr<net::Channel> takeChannel();

    static const char* errorToString(Authenticator::ErrorCode error_code);

protected:
    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    void sendClientHello();
    bool readServerHello(const base::ByteArray& buffer);
    void sendIdentify();
    bool readServerKeyExchange(const base::ByteArray& buffer);
    void sendClientKeyExchange();
    bool readSessionChallenge(const base::ByteArray& buffer);
    void sendSessionResponse();
    void finished(const base::Location& location, ErrorCode error_code);

    enum class State
    {
        NOT_STARTED,
        SEND_CLIENT_HELLO,
        READ_SERVER_HELLO,
        SEND_IDENTIFY,
        READ_SERVER_KEY_EXCHANGE,
        SEND_CLIENT_KEY_EXCHANGE,
        READ_SESSION_CHALLENGE,
        SEND_SESSION_RESPONSE,
        FINISHED
    };

    State state_ = State::NOT_STARTED;

    std::unique_ptr<net::Channel> channel_;
    Callback callback_;

    std::u16string username_;
    std::u16string password_;

    proto::Method method_ = proto::METHOD_UNKNOWN;
    proto::SessionType session_type_ = proto::SESSION_TYPE_UNKNOWN;
    base::Version peer_version_;

    crypto::BigNum N_;
    crypto::BigNum g_;
    crypto::BigNum s_;
    crypto::BigNum B_;

    crypto::BigNum a_;
    crypto::BigNum A_;

    base::ByteArray key_;
    base::ByteArray encrypt_iv_;
    base::ByteArray decrypt_iv_;

    DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

} // namespace client

#endif // CLIENT__CLIENT_AUTHENTICATOR_H
