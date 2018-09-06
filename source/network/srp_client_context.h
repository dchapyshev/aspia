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

#ifndef ASPIA_NETWORK__SRP_CLIENT_CONTEXT_H_
#define ASPIA_NETWORK__SRP_CLIENT_CONTEXT_H_

#include "crypto/big_num.h"
#include "protocol/key_exchange.pb.h"

namespace aspia {

class SrpClientContext
{
public:
    ~SrpClientContext();

    static SrpClientContext* create(proto::Method method,
                                    const std::string& username,
                                    const std::string& password);

    proto::SrpIdentify* identify();

    proto::SrpClientKeyExchange* readServerKeyExchange(
        const proto::SrpServerKeyExchange& server_key_exchange);

    std::string key() const;
    const std::string& encryptIv() const { return encrypt_iv_; }
    const std::string& decryptIv() const { return decrypt_iv_; }

protected:
    SrpClientContext(proto::Method method, const std::string& I, const std::string& p);

private:
    const proto::Method method_;

    std::string I_; // User name.
    std::string p_; // Plain text password.

    BigNum N_;
    BigNum g_;
    BigNum s_;
    BigNum B_;

    BigNum a_;
    BigNum A_;

    std::string encrypt_iv_;
    std::string decrypt_iv_;

    DISALLOW_COPY_AND_ASSIGN(SrpClientContext);
};

} // namespace aspia

#endif // ASPIA_NETWORK__SRP_CLIENT_CONTEXT_H_
