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

#ifndef CLIENT__CLIENT_AUTHENTICATOR_SRP_H
#define CLIENT__CLIENT_AUTHENTICATOR_SRP_H

#include "client/client_authenticator.h"
#include "crypto/big_num.h"

#include <QString>

namespace client {

class AuthenticatorSrp : public Authenticator
{
public:
    AuthenticatorSrp();
    ~AuthenticatorSrp() = default;

    void setUserName(const QString& username);
    const QString& userName() const;

    void setPassword(const QString& password);
    const QString& password() const;

protected:
    // Authenticator implementation.
    uint32_t methods() const override;
    void onStarted() override;
    bool onMessage(const QByteArray& buffer) override;
    std::unique_ptr<crypto::Cryptor> takeCryptor() override;

private:
    QString username_;
    QString password_;

    crypto::BigNum N_;
    crypto::BigNum g_;
    crypto::BigNum s_;
    crypto::BigNum B_;

    crypto::BigNum a_;
    crypto::BigNum A_;

    QByteArray encrypt_iv_;
    QByteArray decrypt_iv_;

    DISALLOW_COPY_AND_ASSIGN(AuthenticatorSrp);
};

} // namespace client

#endif // CLIENT__CLIENT_AUTHENTICATOR_SRP_H
