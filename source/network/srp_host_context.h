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

#ifndef ASPIA_NETWORK__SRP_SERVER_CONTEXT_H
#define ASPIA_NETWORK__SRP_SERVER_CONTEXT_H

#include <QString>

#include "crypto/big_num.h"
#include "protocol/key_exchange.pb.h"

namespace aspia {

struct SrpUserList;
struct SrpUser;

class SrpHostContext
{
public:
    SrpHostContext(proto::Method method, const SrpUserList& user_list);
    ~SrpHostContext();

    static SrpUser* createUser(const QString& username, const QString& password);

    proto::SrpServerKeyExchange* readIdentify(const proto::SrpIdentify& identify);
    void readClientKeyExchange(const proto::SrpClientKeyExchange& client_key_exchange);

    const QString& userName() const { return username_; }
    uint32_t sessionTypes() const { return session_types_; }

    proto::Method method() const { return method_; }
    QByteArray key() const;
    const QByteArray& encryptIv() const { return encrypt_iv_; }
    const QByteArray& decryptIv() const { return decrypt_iv_; }

private:
    const proto::Method method_;

    const SrpUserList& user_list_;

    QString username_;
    uint32_t session_types_ = 0;

    QByteArray encrypt_iv_;
    QByteArray decrypt_iv_;

    BigNum N_;
    BigNum v_;

    BigNum b_;
    BigNum B_;
    BigNum A_;

    DISALLOW_COPY_AND_ASSIGN(SrpHostContext);
};

} // namespace aspia

#endif // ASPIA_NETWORK__SRP_SERVER_CONTEXT_H
