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

#ifndef BASE_PEER_CLIENT_AUTHENTICATOR_H
#define BASE_PEER_CLIENT_AUTHENTICATOR_H

#include "base/crypto/big_num.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/secure_string.h"
#include "base/peer/authenticator.h"

class ClientAuthenticator final : public Authenticator
{
    Q_OBJECT

public:
    explicit ClientAuthenticator(QObject* parent = nullptr);
    ~ClientAuthenticator() final;

    void setPeerPublicKey(const QByteArray& public_key);
    void setIdentify(proto::key_exchange::Identify identify);
    void setUserName(const QString& username);
    void setPassword(const SecureString& password);
    void setSessionType(quint32 session_type);
    void setDisplayName(const QString& display_name);
    void setProbe(bool probe);

    // IDENTIFY_PASSWORD: the public key of the host pinned on a previous connection (empty if the
    // host is not known yet). Credentials are sent only if the host presents exactly this key.
    void setPinnedHostKey(const QByteArray& public_key);

    // IDENTIFY_PASSWORD: the public key presented by the host (available after ServerHello).
    const QByteArray& hostKey() const { return host_key_; }

    // Authenticator implementation.
    [[nodiscard]] bool isRefusedOnClose() const final { return credentials_sent_; }

signals:
    // IDENTIFY_PASSWORD: the host presented a key that is not pinned (|key_changed| is false) or
    // differs from the pinned one (|key_changed| is true). Emitted right before the authenticator
    // finishes with HOST_KEY_UNTRUSTED. No credentials have been sent.
    void sig_hostKeyUntrusted(const QByteArray& host_public_key, bool key_changed);

protected:
    // Authenticator implementation.
    [[nodiscard]] bool onStarted() final;
    void onReceived(const QByteArray& buffer) final;
    [[nodiscard]] QByteArray keyLabel(Direction direction) const final;

private:
    void sendClientHello();
    [[nodiscard]] bool readServerHello(const QByteArray& buffer);
    void sendIdentify();
    void sendPasswordCredentials();
    [[nodiscard]] bool readServerKeyExchange(const QByteArray& buffer);
    void sendClientKeyExchange();
    [[nodiscard]] bool readSessionChallenge(const QByteArray& buffer);
    void sendSessionResponse();

    enum class InternalState
    {
        READ_SERVER_HELLO,
        READ_SERVER_KEY_EXCHANGE,
        READ_SESSION_CHALLENGE
    };

    InternalState internal_state_ = InternalState::READ_SERVER_HELLO;

    QByteArray peer_public_key_;
    QByteArray pinned_host_key_;
    QByteArray host_key_;
    bool credentials_sent_ = false;
    QString username_;
    SecureString password_;
    QString display_name_;

    // Ephemeral X25519 keypair used for the SRP handshake. Created in sendClientHello, used in
    // readServerHello to derive the shared secret with the server's ephemeral public key, then
    // discarded. Not used for ANONYMOUS (which derives the secret immediately in sendClientHello).
    KeyPair key_pair_;

    BigNum N_;
    BigNum g_;
    BigNum s_;
    BigNum B_;
    BigNum a_;
    BigNum A_;

    Q_DISABLE_COPY_MOVE(ClientAuthenticator)
};

#endif // BASE_PEER_CLIENT_AUTHENTICATOR_H
