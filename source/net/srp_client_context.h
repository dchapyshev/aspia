//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_NET__SRP_CLIENT_CONTEXT_H
#define ASPIA_NET__SRP_CLIENT_CONTEXT_H

#include <QString>

#include "crypto/big_num.h"
#include "proto/key_exchange.pb.h"

namespace net {

class SrpClientContext
{
public:
    ~SrpClientContext();

    static SrpClientContext* create(proto::Method method,
                                    const QString& username,
                                    const QString& password);

    proto::SrpIdentify* identify();

    proto::SrpClientKeyExchange* readServerKeyExchange(
        const proto::SrpServerKeyExchange& server_key_exchange);

    proto::Method method() const { return method_; }
    QByteArray key() const;
    const QByteArray& encryptIv() const { return encrypt_iv_; }
    const QByteArray& decryptIv() const { return decrypt_iv_; }

protected:
    SrpClientContext(proto::Method method, const QString& I, const QString& p);

private:
    const proto::Method method_;

    QString I_; // User name.
    QString p_; // Plain text password.

    crypto::BigNum N_;
    crypto::BigNum g_;
    crypto::BigNum s_;
    crypto::BigNum B_;

    crypto::BigNum a_;
    crypto::BigNum A_;

    QByteArray encrypt_iv_;
    QByteArray decrypt_iv_;

    DISALLOW_COPY_AND_ASSIGN(SrpClientContext);
};

} // namespace net

#endif // ASPIA_NET__SRP_CLIENT_CONTEXT_H
