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

#ifndef BASE_PEER_CLIENT_AUTHENTICATOR_H
#define BASE_PEER_CLIENT_AUTHENTICATOR_H

#include "base/crypto/big_num.h"
#include "base/peer/authenticator.h"

namespace base {

class ClientAuthenticator final : public Authenticator
{
    Q_OBJECT

public:
    explicit ClientAuthenticator(QObject* parent = nullptr);
    ~ClientAuthenticator() final;

    void setPeerPublicKey(const QByteArray& public_key);
    void setIdentify(proto::key_exchange::Identify identify);
    void setUserName(const QString& username);
    void setPassword(const QString& password);
    void setSessionType(quint32 session_type);
    void setDisplayName(const QString& display_name);

protected:
    // Authenticator implementation.
    [[nodiscard]] bool onStarted() final;
    void onReceived(const QByteArray& buffer) final;
    void onWritten() final;

private:
    void sendClientHello();
    [[nodiscard]] bool readServerHello(const QByteArray& buffer);
    void sendIdentify();
    [[nodiscard]] bool readServerKeyExchange(const QByteArray& buffer);
    void sendClientKeyExchange();
    [[nodiscard]] bool readSessionChallenge(const QByteArray& buffer);
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

    QByteArray peer_public_key_;
    QString username_;
    QString password_;
    QString display_name_;

    BigNum N_;
    BigNum g_;
    BigNum s_;
    BigNum B_;
    BigNum a_;
    BigNum A_;

    Q_DISABLE_COPY(ClientAuthenticator)
};

} // namespace base

#endif // BASE_PEER_CLIENT_AUTHENTICATOR_H
