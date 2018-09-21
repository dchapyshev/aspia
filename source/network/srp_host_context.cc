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

#include "network/srp_host_context.h"

#include "base/logging.h"
#include "crypto/generic_hash.h"
#include "crypto/random.h"
#include "crypto/secure_memory.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"
#include "network/srp_user.h"

namespace aspia {

namespace {

const size_t kUserSaltSize = 64; // In bytes.

// Looks for the user in the list.
// If the user is found, it returns an index in the list.
// If the user is not found or disabled, -1 is returned.
size_t findUser(const SrpUserList& users, const std::string& username)
{
    for (size_t i = 0; i < users.list.size(); ++i)
    {
        const SrpUser& user = users.list.at(i);

        if (user.name == username)
        {
            // If the user is disabled, we assume that it was not found.
            if (!(user.flags & SrpUser::ENABLED))
                return -1;

            return i;
        }
    }

    return -1;
}

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

SrpHostContext::SrpHostContext(proto::Method method, std::shared_ptr<SrpUserList> user_list)
    : method_(method),
      user_list_(user_list)
{
    DCHECK(user_list_);
}

SrpHostContext::~SrpHostContext()
{
    secureMemZero(&encrypt_iv_);
    secureMemZero(&decrypt_iv_);
}

// static
SrpUser* SrpHostContext::createUser(const std::string& username, const std::string& password)
{
    std::unique_ptr<SrpUser> user = std::make_unique<SrpUser>();

    user->name = username;
    user->salt = Random::generateBuffer(kUserSaltSize);

    user->number = std::string(
        reinterpret_cast<const char*>(kSrpNg_8192.N.data()), kSrpNg_8192.N.size());
    user->generator = std::string(
        reinterpret_cast<const char*>(kSrpNg_8192.g.data()), kSrpNg_8192.g.size());

    if (user->salt.empty())
        return nullptr;

    BigNum s = BigNum::fromStdString(user->salt);
    BigNum N = BigNum::fromStdString(user->number);
    BigNum g = BigNum::fromStdString(user->generator);

    BigNum v = SrpMath::calc_v(username, password, s, N, g);

    user->verifier = v.toStdString();
    if (user->verifier.empty())
        return nullptr;

    return user.release();
}

proto::SrpServerKeyExchange* SrpHostContext::readIdentify(const proto::SrpIdentify& identify)
{
    if (identify.username().empty())
        return nullptr;

    BigNum g;
    BigNum s;

    size_t user_index = findUser(*user_list_, identify.username());
    if (user_index == -1)
    {
        username_ = identify.username();
        session_types_ = proto::SESSION_TYPE_ALL;

        GenericHash hash(GenericHash::BLAKE2b512);
        hash.addData(user_list_->seed_key);
        hash.addData(identify.username());

        N_ = BigNum::fromBuffer(kSrpNg_8192.N);
        g = BigNum::fromBuffer(kSrpNg_8192.g);
        s = BigNum::fromStdString(hash.result());
        v_ = SrpMath::calc_v(identify.username(), user_list_->seed_key, s, N_, g);
    }
    else
    {
        const SrpUser& user = user_list_->list.at(user_index);

        username_ = user.name;
        session_types_ = user.sessions;

        N_ = BigNum::fromStdString(user.number);
        g = BigNum::fromStdString(user.generator);
        s = BigNum::fromStdString(user.salt);
        v_ = BigNum::fromStdString(user.verifier);
    }

    uint8_t random_buffer[128];
    if (!Random::fillBuffer(random_buffer, sizeof(random_buffer)))
        return nullptr;

    b_ = BigNum::fromBuffer(ConstBuffer(random_buffer, sizeof(random_buffer)));
    B_ = SrpMath::calc_B(b_, N_, g, v_);

    if (!N_.isValid() || !g.isValid() || !s.isValid() || !B_.isValid())
        return nullptr;

    size_t iv_size = ivSizeForMethod(method_);
    if (!iv_size)
        return nullptr;

    encrypt_iv_ = Random::generateBuffer(iv_size);
    if (encrypt_iv_.empty())
        return nullptr;

    std::unique_ptr<proto::SrpServerKeyExchange> server_key_exchange =
        std::make_unique<proto::SrpServerKeyExchange>();

    server_key_exchange->set_number(N_.toStdString());
    server_key_exchange->set_generator(g.toStdString());
    server_key_exchange->set_salt(s.toStdString());
    server_key_exchange->set_b(B_.toStdString());
    server_key_exchange->set_iv(encrypt_iv_);

    return server_key_exchange.release();
}

void SrpHostContext::readClientKeyExchange(const proto::SrpClientKeyExchange& client_key_exchange)
{
    A_ = BigNum::fromStdString(client_key_exchange.a());
    decrypt_iv_ = client_key_exchange.iv();
}

std::string SrpHostContext::key() const
{
    if (!SrpMath::verify_A_mod_N(A_, N_))
    {
        LOG(LS_WARNING) << "SrpMath::verify_A_mod_N failed";
        return std::string();
    }

    BigNum u = SrpMath::calc_u(A_, B_, N_);
    BigNum server_key = SrpMath::calcServerKey(A_, v_, u, b_, N_);

    switch (method_)
    {
        // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
        {
            std::string server_key_string = server_key.toStdString();
            std::string result = GenericHash::hash(GenericHash::BLAKE2s256, server_key_string);
            secureMemZero(&server_key_string);
            return result;
        }

        default:
        {
            LOG(LS_WARNING) << "Unknown method: " << method_;
            return std::string();
        }
    }
}

} // namespace aspia
