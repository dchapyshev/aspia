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

#ifndef PEER__CLIENT_AUTHENTICATOR_H
#define PEER__CLIENT_AUTHENTICATOR_H

#include "base/crypto/big_num.h"
#include "peer/authenticator.h"

#include <functional>

namespace base {
class Location;
class MessageDecryptor;
class MessageEncryptor;
} // namespace base

namespace peer {

class ClientAuthenticator : public Authenticator
{
public:
    explicit ClientAuthenticator(std::shared_ptr<base::TaskRunner> task_runner);
    ~ClientAuthenticator();

    void setPeerPublicKey(const base::ByteArray& public_key);
    void setIdentify(proto::Identify identify);
    void setUserName(std::u16string_view username);
    void setPassword(std::u16string_view password);
    void setSessionType(uint32_t session_type);

protected:
    // Authenticator implementation.
    bool onStarted() override;
    void onReceived(const base::ByteArray& buffer) override;
    void onWritten() override;

private:
    void sendClientHello();
    bool readServerHello(const base::ByteArray& buffer);
    void sendIdentify();
    bool readServerKeyExchange(const base::ByteArray& buffer);
    void sendClientKeyExchange();
    bool readSessionChallenge(const base::ByteArray& buffer);
    void sendSessionResponse();

    enum class InternalState
    {
        SEND_CLIENT_HELLO,
        READ_SERVER_HELLO,
        SEND_IDENTIFY,
        READ_SERVER_KEY_EXCHANGE,
        SEND_CLIENT_KEY_EXCHANGE,
        READ_SESSION_CHALLENGE,
        SEND_SESSION_RESPONSE
    };

    InternalState internal_state_ = InternalState::SEND_CLIENT_HELLO;

    base::ByteArray peer_public_key_;
    std::u16string username_;
    std::u16string password_;

    base::BigNum N_;
    base::BigNum g_;
    base::BigNum s_;
    base::BigNum B_;
    base::BigNum a_;
    base::BigNum A_;

    DISALLOW_COPY_AND_ASSIGN(ClientAuthenticator);
};

} // namespace peer

#endif // PEER__CLIENT_AUTHENTICATOR_H
