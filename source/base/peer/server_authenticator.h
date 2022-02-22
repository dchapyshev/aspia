//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_PEER_SERVER_AUTHENTICATOR_H
#define BASE_PEER_SERVER_AUTHENTICATOR_H

#include "base/crypto/big_num.h"
#include "base/crypto/key_pair.h"
#include "base/memory/local_memory.h"
#include "base/net/network_channel.h"
#include "base/peer/authenticator.h"

namespace base {

class UserListBase;

class ServerAuthenticator : public Authenticator
{
public:
    explicit ServerAuthenticator(std::shared_ptr<TaskRunner> task_runner);
    ~ServerAuthenticator() override;

    enum class AnonymousAccess
    {
        ENABLE, // Anonymous access is enabled.
        DISABLE // Anonymous access is disabled.
    };

    // Sets the user list.
    void setUserList(base::local_shared_ptr<UserListBase> user_list);

    // Sets the private key.
    [[nodiscard]] bool setPrivateKey(const ByteArray& private_key);

    // Enables or disables anonymous access.
    // |session_types] allowed session types for anonymous access.
    // The private key must be set up for anonymous access.
    // By default, anonymous access is disabled.
    [[nodiscard]] bool setAnonymousAccess(AnonymousAccess anonymous_access, uint32_t session_types);

protected:
    // Authenticator implementation.
    bool onStarted() override;
    void onReceived(const ByteArray& buffer) override;
    void onWritten() override;

private:
    void onClientHello(const ByteArray& buffer);
    void onIdentify(const ByteArray& buffer);
    void onClientKeyExchange(const ByteArray& buffer);
    void doSessionChallenge();
    void onSessionResponse(const ByteArray& buffer);
    [[nodiscard]] ByteArray createSrpKey();

    base::local_shared_ptr<UserListBase> user_list_;

    enum class InternalState
    {
        READ_CLIENT_HELLO,
        SEND_SERVER_HELLO,
        READ_IDENTIFY,
        SEND_SERVER_KEY_EXCHANGE,
        READ_CLIENT_KEY_EXCHANGE,
        SEND_SESSION_CHALLENGE,
        READ_SESSION_RESPONSE
    };

    AnonymousAccess anonymous_access_ = AnonymousAccess::DISABLE;
    InternalState internal_state_ = InternalState::READ_CLIENT_HELLO;

    // Bitmask of allowed session types.
    uint32_t session_types_ = 0;

    KeyPair key_pair_;
    BigNum N_;
    BigNum g_;
    BigNum v_;
    BigNum s_;
    BigNum b_;
    BigNum B_;
    BigNum A_;

    DISALLOW_COPY_AND_ASSIGN(ServerAuthenticator);
};

} // namespace base

#endif // BASE_PEER_SERVER_AUTHENTICATOR_H
