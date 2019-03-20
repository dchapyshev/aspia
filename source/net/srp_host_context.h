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

#ifndef NET__SRP_SERVER_CONTEXT_H
#define NET__SRP_SERVER_CONTEXT_H

#include "crypto/big_num.h"
#include "proto/key_exchange.pb.h"

#include <QString>

namespace net {

class SrpUserList;
class SrpUser;

class SrpHostContext
{
public:
    SrpHostContext(proto::Method method, const SrpUserList& user_list);
    ~SrpHostContext();

    proto::SrpServerKeyExchange* readIdentify(const proto::SrpIdentify& identify);
    void readClientKeyExchange(const proto::SrpClientKeyExchange& client_key_exchange);

    const std::string& userName() const { return username_; }
    uint32_t sessionTypes() const { return session_types_; }

    proto::Method method() const { return method_; }
    QByteArray key() const;
    const QByteArray& encryptIv() const { return encrypt_iv_; }
    const QByteArray& decryptIv() const { return decrypt_iv_; }

private:
    const proto::Method method_;

    const SrpUserList& user_list_;

    std::string username_;
    uint32_t session_types_ = 0;

    QByteArray encrypt_iv_;
    QByteArray decrypt_iv_;

    crypto::BigNum N_;
    crypto::BigNum v_;

    crypto::BigNum b_;
    crypto::BigNum B_;
    crypto::BigNum A_;

    DISALLOW_COPY_AND_ASSIGN(SrpHostContext);
};

} // namespace net

#endif // NET__SRP_SERVER_CONTEXT_H
