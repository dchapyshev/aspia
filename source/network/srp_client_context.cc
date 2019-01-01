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

#include "network/srp_client_context.h"

#include "base/logging.h"
#include "crypto/generic_hash.h"
#include "crypto/random.h"
#include "crypto/secure_memory.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"

namespace aspia {

namespace {

bool verifyNg(const std::string& N, const std::string& g)
{
    switch (N.size())
    {
        case 256: // 2048 bit
        {
            if (memcmp(N.data(), kSrpNg_2048.N.data(), kSrpNg_2048.N.size()) != 0)
                return false;

            if (g.size() != kSrpNg_2048.g.size())
                return false;

            if (memcmp(g.data(), kSrpNg_2048.g.data(), kSrpNg_2048.g.size()) != 0)
                return false;
        }
        break;

        case 384: // 3072 bit
        {
            if (memcmp(N.data(), kSrpNg_3072.N.data(), kSrpNg_3072.N.size()) != 0)
                return false;

            if (g.size() != kSrpNg_3072.g.size())
                return false;

            if (memcmp(g.data(), kSrpNg_3072.g.data(), kSrpNg_3072.g.size()) != 0)
                return false;
        }
        break;

        case 512: // 4096 bit
        {
            if (memcmp(N.data(), kSrpNg_4096.N.data(), kSrpNg_4096.N.size()) != 0)
                return false;

            if (g.size() != kSrpNg_4096.g.size())
                return false;

            if (memcmp(g.data(), kSrpNg_4096.g.data(), kSrpNg_4096.g.size()) != 0)
                return false;
        }
        break;

        case 768: // 6144 bit
        {
            if (memcmp(N.data(), kSrpNg_6144.N.data(), kSrpNg_6144.N.size()) != 0)
                return false;

            if (g.size() != kSrpNg_6144.g.size())
                return false;

            if (memcmp(g.data(), kSrpNg_6144.g.data(), kSrpNg_6144.g.size()) != 0)
                return false;
        }
        break;

        case 1024: // 8192 bit
        {
            if (memcmp(N.data(), kSrpNg_8192.N.data(), kSrpNg_8192.N.size()) != 0)
                return false;

            if (g.size() != kSrpNg_8192.g.size())
                return false;

            if (memcmp(g.data(), kSrpNg_8192.g.data(), kSrpNg_8192.g.size()) != 0)
                return false;
        }
        break;

        // We do not allow groups with a length of 1024 and 1536 bits.
        case 128:
        case 192:
        default:
            return false;
    }

    return true;
}

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

SrpClientContext::SrpClientContext(proto::Method method,
                                   const QString& I,
                                   const QString& p)
    : method_(method),
      I_(I),
      p_(p)
{
    // Nothing
}

SrpClientContext::~SrpClientContext()
{
    secureMemZero(&p_);
    secureMemZero(&encrypt_iv_);
    secureMemZero(&decrypt_iv_);
}

// static
SrpClientContext* SrpClientContext::create(proto::Method method,
                                           const QString& username,
                                           const QString& password)
{
    switch (method)
    {
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
            break;

        default:
            return nullptr;
    }

    if (username.isEmpty() || password.isEmpty())
        return nullptr;

    return new SrpClientContext(method, username, password);
}

proto::SrpIdentify* SrpClientContext::identify()
{
    std::unique_ptr<proto::SrpIdentify> identify =
        std::make_unique<proto::SrpIdentify>();

    identify->set_username(I_.toStdString());

    return identify.release();
}

proto::SrpClientKeyExchange* SrpClientContext::readServerKeyExchange(
    const proto::SrpServerKeyExchange& server_key_exchange)
{
    static const size_t kMin_s = 64;
    static const size_t kMin_B = 128;

    if (server_key_exchange.salt().size() < kMin_s)
    {
        LOG(LS_WARNING) << "Wrong salt size:" << server_key_exchange.salt().size();
        return nullptr;
    }

    if (server_key_exchange.b().size() < kMin_B)
    {
        LOG(LS_WARNING) << "Wrong B size:" << server_key_exchange.b().size();
        return nullptr;
    }

    if (!verifyNg(server_key_exchange.number(), server_key_exchange.generator()))
    {
        LOG(LS_WARNING) << "Wrong number or generator";
        return nullptr;
    }

    N_ = BigNum::fromStdString(server_key_exchange.number());
    g_ = BigNum::fromStdString(server_key_exchange.generator());
    s_ = BigNum::fromStdString(server_key_exchange.salt());
    B_ = BigNum::fromStdString(server_key_exchange.b());
    decrypt_iv_ = QByteArray::fromStdString(server_key_exchange.iv());

    a_ = BigNum::fromByteArray(Random::generateBuffer(128)); // 1024 bits.
    A_ = SrpMath::calc_A(a_, N_, g_);

    size_t iv_size = ivSizeForMethod(method_);
    if (!iv_size)
        return nullptr;

    encrypt_iv_ = Random::generateBuffer(iv_size);

    std::unique_ptr<proto::SrpClientKeyExchange> client_key_exchange =
        std::make_unique<proto::SrpClientKeyExchange>();

    client_key_exchange->set_a(A_.toStdString());
    client_key_exchange->set_iv(encrypt_iv_.toStdString());

    return client_key_exchange.release();
}

QByteArray SrpClientContext::key() const
{
    if (!SrpMath::verify_B_mod_N(B_, N_))
    {
        LOG(LS_WARNING) << "Invalid B or N";
        return QByteArray();
    }

    BigNum u = SrpMath::calc_u(A_, B_, N_);
    BigNum x = SrpMath::calc_x(s_, I_.toUtf8(), p_.toUtf8());
    BigNum client_key = SrpMath::calcClientKey(N_, B_, g_, x, a_, u);

    QByteArray client_key_string = client_key.toByteArray();
    if (client_key_string.isEmpty())
    {
        LOG(LS_WARNING) << "Empty encryption key generated";
        return QByteArray();
    }

    switch (method_)
    {
        // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
            return GenericHash::hash(GenericHash::BLAKE2s256, client_key_string);

        default:
            LOG(LS_WARNING) << "Unknown encryption method: " << method_;
            return QByteArray();
    }
}

} // namespace aspia
