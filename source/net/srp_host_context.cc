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

#include "net/srp_host_context.h"
#include "base/logging.h"
#include "crypto/generic_hash.h"
#include "crypto/random.h"
#include "crypto/secure_memory.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"
#include "net/srp_user.h"

namespace net {

namespace {

// Returns the size of the initialization vector for the specified method.
// If the method is not supported, it returns 0.
size_t ivSizeForMethod(proto::Method method)
{
    switch (method)
    {
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
            return 12;

        default:
            return 0;
    }
}

} // namespace

SrpHostContext::SrpHostContext(proto::Method method, const SrpUserList& user_list)
    : method_(method),
      user_list_(user_list)
{
    // Nothing
}

SrpHostContext::~SrpHostContext()
{
    crypto::memZero(&encrypt_iv_);
    crypto::memZero(&decrypt_iv_);
}

proto::SrpServerKeyExchange* SrpHostContext::readIdentify(const proto::SrpIdentify& identify)
{
    username_ = identify.username();
    if (username_.empty())
        return nullptr;

    crypto::BigNum g;
    crypto::BigNum s;

    int user_index = user_list_.find(QString::fromStdString(username_));
    if (user_index == -1)
    {
        session_types_ = proto::SESSION_TYPE_ALL;

        crypto::GenericHash hash(crypto::GenericHash::BLAKE2b512);
        hash.addData(user_list_.seedKey());
        hash.addData(identify.username());

        N_ = crypto::BigNum::fromBuffer(crypto::kSrpNg_8192.N);
        g = crypto::BigNum::fromBuffer(crypto::kSrpNg_8192.g);
        s = crypto::BigNum::fromByteArray(hash.result());
        v_ = crypto::SrpMath::calc_v(username_, user_list_.seedKey().toStdString(), s, N_, g);
    }
    else
    {
        const SrpUser& user = user_list_.at(user_index);

        session_types_ = user.sessions;

        N_ = crypto::BigNum::fromByteArray(user.number);
        g = crypto::BigNum::fromByteArray(user.generator);
        s = crypto::BigNum::fromByteArray(user.salt);
        v_ = crypto::BigNum::fromByteArray(user.verifier);
    }

    b_ = crypto::BigNum::fromByteArray(crypto::Random::generateBuffer(128)); // 1024 bits.
    B_ = crypto::SrpMath::calc_B(b_, N_, g, v_);

    if (!N_.isValid() || !g.isValid() || !s.isValid() || !B_.isValid())
        return nullptr;

    size_t iv_size = ivSizeForMethod(method_);
    if (!iv_size)
        return nullptr;

    encrypt_iv_ = crypto::Random::generateBuffer(iv_size);

    std::unique_ptr<proto::SrpServerKeyExchange> server_key_exchange =
        std::make_unique<proto::SrpServerKeyExchange>();

    server_key_exchange->set_number(N_.toStdString());
    server_key_exchange->set_generator(g.toStdString());
    server_key_exchange->set_salt(s.toStdString());
    server_key_exchange->set_b(B_.toStdString());
    server_key_exchange->set_iv(encrypt_iv_.toStdString());

    return server_key_exchange.release();
}

void SrpHostContext::readClientKeyExchange(const proto::SrpClientKeyExchange& client_key_exchange)
{
    A_ = crypto::BigNum::fromStdString(client_key_exchange.a());
    decrypt_iv_ = QByteArray::fromStdString(client_key_exchange.iv());
}

QByteArray SrpHostContext::key() const
{
    if (!crypto::SrpMath::verify_A_mod_N(A_, N_))
    {
        LOG(LS_WARNING) << "SrpMath::verify_A_mod_N failed";
        return QByteArray();
    }

    crypto::BigNum u = crypto::SrpMath::calc_u(A_, B_, N_);
    crypto::BigNum server_key = crypto::SrpMath::calcServerKey(A_, v_, u, b_, N_);

    switch (method_)
    {
        // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
        {
            QByteArray server_key_string = server_key.toByteArray();
            QByteArray result = crypto::GenericHash::hash(
                crypto::GenericHash::BLAKE2s256, server_key_string);
            crypto::memZero(&server_key_string);
            return result;
        }

        default:
        {
            LOG(LS_WARNING) << "Unknown method: " << method_;
            return QByteArray();
        }
    }
}

} // namespace net
