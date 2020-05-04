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

#ifndef NET__CLIENT_AUTHENTICATOR_H
#define NET__CLIENT_AUTHENTICATOR_H

#include "base/version.h"
#include "crypto/big_num.h"
#include "net/channel.h"
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

class ClientAuthenticator : public Channel::Listener
{
public:
    ClientAuthenticator();
    ~ClientAuthenticator();

    void setPeerPublicKey(const base::ByteArray& public_key);
    const base::ByteArray& peerPublicKey() const { return peer_public_key_; }

    void setIdentify(proto::Identify identify);
    proto::Identify identify() const { return identify_; }

    void setUserName(std::u16string_view username);
    const std::u16string& userName() const;

    void setPassword(std::u16string_view password);
    const std::u16string& password() const;

    void setSessionType(uint32_t session_type);
    uint32_t sessionType() const { return session_type_; }

    proto::Encryption encryption() const { return encryption_; }
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
    void start(std::unique_ptr<Channel> channel, Callback callback);

    std::unique_ptr<Channel> takeChannel();

    static const char* errorToString(ClientAuthenticator::ErrorCode error_code);

protected:
    // Channel::Listener implementation.
    void onConnected() override;
    void onDisconnected(Channel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void onSessionKeyChanged();
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

    std::unique_ptr<Channel> channel_;
    Callback callback_;

    base::ByteArray peer_public_key_;
    std::u16string username_;
    std::u16string password_;

    proto::Encryption encryption_ = proto::ENCRYPTION_UNKNOWN;
    proto::Identify identify_ = proto::IDENTIFY_SRP;
    uint32_t session_type_ = 0;
    base::Version peer_version_;

    crypto::BigNum N_;
    crypto::BigNum g_;
    crypto::BigNum s_;
    crypto::BigNum B_;

    crypto::BigNum a_;
    crypto::BigNum A_;

    base::ByteArray session_key_;
    base::ByteArray encrypt_iv_;
    base::ByteArray decrypt_iv_;

    DISALLOW_COPY_AND_ASSIGN(ClientAuthenticator);
};

} // namespace net

#endif // NET__CLIENT_AUTHENTICATOR_H
