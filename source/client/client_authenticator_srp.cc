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

#include "client/client_authenticator_srp.h"

#include "base/cpuid.h"
#include "common/message_serialization.h"
#include "crypto/cryptor_aes256_gcm.h"
#include "crypto/cryptor_chacha20_poly1305.h"
#include "crypto/generic_hash.h"
#include "crypto/random.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"

namespace client {

namespace {

bool verifyNg(const std::string& N, const std::string& g)
{
    switch (N.size())
    {
        case 512: // 4096 bit
        {
            if (memcmp(N.data(), crypto::kSrpNg_4096.N.data(), crypto::kSrpNg_4096.N.size()) != 0)
                return false;

            if (g.size() != crypto::kSrpNg_4096.g.size())
                return false;

            if (memcmp(g.data(), crypto::kSrpNg_4096.g.data(), crypto::kSrpNg_4096.g.size()) != 0)
                return false;
        }
        break;

        case 768: // 6144 bit
        {
            if (memcmp(N.data(), crypto::kSrpNg_6144.N.data(), crypto::kSrpNg_6144.N.size()) != 0)
                return false;

            if (g.size() != crypto::kSrpNg_6144.g.size())
                return false;

            if (memcmp(g.data(), crypto::kSrpNg_6144.g.data(), crypto::kSrpNg_6144.g.size()) != 0)
                return false;
        }
        break;

        case 1024: // 8192 bit
        {
            if (memcmp(N.data(), crypto::kSrpNg_8192.N.data(), crypto::kSrpNg_8192.N.size()) != 0)
                return false;

            if (g.size() != crypto::kSrpNg_8192.g.size())
                return false;

            if (memcmp(g.data(), crypto::kSrpNg_8192.g.data(), crypto::kSrpNg_8192.g.size()) != 0)
                return false;
        }
        break;

        // We do not allow groups less than 512 bytes (4096 bits).
        default:
            return false;
    }

    return true;
}

} // namespace

AuthenticatorSrp::AuthenticatorSrp() = default;

void AuthenticatorSrp::setUserName(const QString& username)
{
    username_ = username;
}

const QString& AuthenticatorSrp::userName() const
{
    return username_;
}

void AuthenticatorSrp::setPassword(const QString& password)
{
    password_ = password;
}

const QString& AuthenticatorSrp::password() const
{
    return password_;
}

uint32_t AuthenticatorSrp::methods() const
{
    uint32_t methods = proto::METHOD_SRP_CHACHA20_POLY1305;

    if (base::CPUID::hasAesNi())
        methods |= proto::METHOD_SRP_AES256_GCM;

    return methods;
}

void AuthenticatorSrp::onStarted()
{
    switch (method())
    {
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
            break;

        default:
            onFinished(Result::PROTOCOL_ERROR);
            return;
    }

    proto::SrpIdentify identify;
    identify.set_username(username_.toStdString());
    sendMessage(common::serializeMessage(identify));
}

bool AuthenticatorSrp::onMessage(const QByteArray& buffer)
{
    proto::SrpServerKeyExchange server_key_exchange;
    if (!common::parseMessage(buffer, server_key_exchange))
    {
        onFinished(Result::PROTOCOL_ERROR);
        return false;
    }

    if (server_key_exchange.salt().size() < 64 || server_key_exchange.b().size() < 128)
    {
        onFinished(Result::PROTOCOL_ERROR);
        return false;
    }

    if (!verifyNg(server_key_exchange.number(), server_key_exchange.generator()))
    {
        onFinished(Result::PROTOCOL_ERROR);
        return false;
    }

    N_ = crypto::BigNum::fromStdString(server_key_exchange.number());
    g_ = crypto::BigNum::fromStdString(server_key_exchange.generator());
    s_ = crypto::BigNum::fromStdString(server_key_exchange.salt());
    B_ = crypto::BigNum::fromStdString(server_key_exchange.b());
    decrypt_iv_ = QByteArray::fromStdString(server_key_exchange.iv());

    a_ = crypto::BigNum::fromByteArray(crypto::Random::byteArray(128)); // 1024 bits.
    A_ = crypto::SrpMath::calc_A(a_, N_, g_);
    encrypt_iv_ = crypto::Random::byteArray(12);

    proto::SrpClientKeyExchange client_key_exchange;
    client_key_exchange.set_a(A_.toStdString());
    client_key_exchange.set_iv(encrypt_iv_.toStdString());

    sendMessage(common::serializeMessage(client_key_exchange));
    return true;
}

std::unique_ptr<crypto::Cryptor> AuthenticatorSrp::takeCryptor()
{
    if (!crypto::SrpMath::verify_B_mod_N(B_, N_))
    {
        LOG(LS_WARNING) << "Invalid B or N";
        return nullptr;
    }

    crypto::BigNum u = crypto::SrpMath::calc_u(A_, B_, N_);
    crypto::BigNum x = crypto::SrpMath::calc_x(s_, username_, password_);
    crypto::BigNum key = crypto::SrpMath::calcClientKey(N_, B_, g_, x, a_, u);
    if (!key.isValid())
    {
        LOG(LS_WARNING) << "Empty encryption key generated";
        return nullptr;
    }

    // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
    QByteArray key_buffer =
        crypto::GenericHash::hash(crypto::GenericHash::BLAKE2s256, key.toByteArray());

    switch (method())
    {
        case proto::METHOD_SRP_AES256_GCM:
            return crypto::CryptorAes256Gcm::create(
                std::move(key_buffer), std::move(encrypt_iv_), std::move(decrypt_iv_));

        case proto::METHOD_SRP_CHACHA20_POLY1305:
            return crypto::CryptorChaCha20Poly1305::create(
                std::move(key_buffer), std::move(encrypt_iv_), std::move(decrypt_iv_));

        default:
            NOTREACHED();
            return nullptr;
    }
}

} // namespace client
