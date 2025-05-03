//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/net/tcp_channel.h"
#include "base/peer/authenticator.h"

namespace base {

class UserListBase;

class ServerAuthenticator final : public Authenticator
{
    Q_OBJECT

public:
    explicit ServerAuthenticator(QObject* parent = nullptr);
    ~ServerAuthenticator() final;

    enum class AnonymousAccess
    {
        ENABLE, // Anonymous access is enabled.
        DISABLE // Anonymous access is disabled.
    };

    // Sets the user list.
    void setUserList(base::local_shared_ptr<UserListBase> user_list);

    // Sets the private key.
    [[nodiscard]] bool setPrivateKey(const QByteArray& private_key);

    // Enables or disables anonymous access.
    // |session_types] allowed session types for anonymous access.
    // The private key must be set up for anonymous access.
    // By default, anonymous access is disabled.
    [[nodiscard]] bool setAnonymousAccess(AnonymousAccess anonymous_access, quint32 session_types);

protected:
    // Authenticator implementation.
    [[nodiscard]] bool onStarted() final;
    void onReceived(const QByteArray& buffer) final;
    void onWritten() final;

private:
    void onClientHello(const QByteArray& buffer);
    void onIdentify(const QByteArray& buffer);
    void onClientKeyExchange(const QByteArray& buffer);
    void doSessionChallenge();
    void onSessionResponse(const QByteArray& buffer);
    [[nodiscard]] QByteArray createSrpKey();

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
    quint32 session_types_ = 0;

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
