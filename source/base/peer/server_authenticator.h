//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QElapsedTimer>

#include <atomic>
#include <memory>

#include "base/shared_pointer.h"
#include "base/crypto/big_num.h"
#include "base/crypto/key_pair.h"
#include "base/peer/authenticator.h"
#include "base/peer/domain_authenticator.h"
#include "base/peer/user_list.h"

class DomainAuthLimiter;

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
    void setUserList(SharedPointer<UserList> user_list);

    // Sets the private key.
    [[nodiscard]] bool setPrivateKey(const SecureByteArray& private_key);

    // Enables or disables anonymous access.
    // |session_types] allowed session types for anonymous access.
    // The private key must be set up for anonymous access.
    // By default, anonymous access is disabled.
    [[nodiscard]] bool setAnonymousAccess(AnonymousAccess anonymous_access, quint32 session_types);

    // Address of the client ("192.0.2.1", "2001:db8::1"). Used to limit failed operating system /
    // domain logons per source and passed to the operating system for its logs and policies.
    void setPeerAddress(const QString& address);

    // Persistent identity key of the host, used only by IDENTIFY_PASSWORD (the client pins its
    // public key). It is separate from the key of setPrivateKey(), which serves anonymous access
    // and the router; the identity key never enables or signs anything else.
    [[nodiscard]] bool setIdentityKey(const SecureByteArray& private_key);

protected:
    // Authenticator implementation.
    [[nodiscard]] bool onStarted() final;
    void onReceived(const QByteArray& buffer) final;
    [[nodiscard]] QByteArray keyLabel(Direction direction) const final;

private:
    void onClientHello(const QByteArray& buffer);
    void onIdentify(const QByteArray& buffer);
    void onClientKeyExchange(const QByteArray& buffer);
    void onPasswordCredentials(const QByteArray& buffer);
    void onDomainAuthFinished(const DomainAuthenticator::Result& result);
    void finishDomainAuthFailure();
    void finishMalformedCredentials(const char* reason);
    void cancelDomainLogon();
    void doSessionChallenge();
    void onSessionResponse(const QByteArray& buffer);
    [[nodiscard]] QByteArray createSrpKey();

    SharedPointer<UserList> user_list_;
    std::shared_ptr<const DomainAuthenticator> domain_authenticator_;
    std::shared_ptr<DomainAuthLimiter> domain_auth_limiter_;

    // Set when the handshake ends, so that a logon still waiting in the queue is skipped.
    std::shared_ptr<std::atomic_bool> logon_cancel_;

    QString peer_address_;

    // Started when PasswordCredentials is received. Failures are answered at a fixed time after it.
    QElapsedTimer credentials_timer_;

    enum class InternalState
    {
        READ_CLIENT_HELLO,
        READ_IDENTIFY,
        READ_CLIENT_KEY_EXCHANGE,
        READ_PASSWORD_CREDENTIALS,
        WAIT_DOMAIN_AUTH,
        READ_SESSION_RESPONSE
    };

    AnonymousAccess anonymous_access_ = AnonymousAccess::DISABLE;
    InternalState internal_state_ = InternalState::READ_CLIENT_HELLO;

    // Bitmask of allowed session types.
    quint32 session_types_ = 0;

    KeyPair key_pair_;
    KeyPair identity_key_pair_;

    BigNum N_;
    BigNum g_;
    BigNum v_;
    BigNum s_;
    BigNum b_;
    BigNum B_;
    BigNum A_;

    Q_DISABLE_COPY_MOVE(ServerAuthenticator)
};

#endif // BASE_PEER_SERVER_AUTHENTICATOR_H
