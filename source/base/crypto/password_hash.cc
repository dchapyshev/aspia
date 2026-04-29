//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "crypto/password_hash.h"

#include "base/logging.h"

#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
template <typename InputT, typename OutputT>
OutputT hashScrypt(const QString& password, InputT salt)
{
    // CPU/Memory cost parameter, must be larger than 1, a power of 2, and less than 2^(128 * r / 8).
    static const quint64 N = 16384;

    // Block size parameter.
    static const quint64 r = 8;

    // Parallelization parameter, a positive integer less than or equal to ((2^32-1) * hLen) / MFLen
    // where hLen is 32 and MFlen is 128 * r.
    static const quint64 p = 2;

    // 32MB
    static const quint64 max_mem = 32 * 1024 * 1024;

    OutputT result;
    result.resize(PasswordHash::kBytesSize);

    QByteArray password_utf8 = password.toUtf8();

    int ret = EVP_PBE_scrypt(password_utf8.data(), password_utf8.size(),
                             reinterpret_cast<const quint8*>(salt.data()), salt.size(),
                             N, r, p, max_mem,
                             reinterpret_cast<quint8*>(result.data()), result.size());
    CHECK_EQ(ret, 1) << "EVP_PBE_scrypt failed";

    return result;
}

//--------------------------------------------------------------------------------------------------
template <typename InputT, typename OutputT>
OutputT hashArgon2id(const QString& password, InputT salt)
{
    static const quint32 kMemoryCost = 131072; // 128 MiB
    static const quint32 kIterations = 4;
    static const quint32 kLanes = 1;
    static const quint32 kThreads = 1;

    OutputT result;
    result.resize(PasswordHash::kBytesSize);

    QByteArray password_utf8 = password.toUtf8();

    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    CHECK(kdf) << "EVP_KDF_fetch(ARGON2ID) failed";

    EVP_KDF_CTX* kdf_ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    CHECK(kdf_ctx) << "EVP_KDF_CTX_new failed";

    quint32 memcost_v = kMemoryCost;
    quint32 iter_v = kIterations;
    quint32 lanes_v = kLanes;
    quint32 threads_v = kThreads;

    OSSL_PARAM params[] =
    {
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                password_utf8.data(),
                                static_cast<size_t>(password_utf8.size())),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SALT,
                                const_cast<char*>(salt.data()),
                                static_cast<size_t>(salt.size())),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_THREADS, &threads_v),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes_v),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memcost_v),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_ITER, &iter_v),
        OSSL_PARAM_END
    };

    int ret = EVP_KDF_derive(kdf_ctx,
                             reinterpret_cast<quint8*>(result.data()),
                             static_cast<size_t>(result.size()),
                             params);
    EVP_KDF_CTX_free(kdf_ctx);
    CHECK_EQ(ret, 1) << "EVP_KDF_derive(ARGON2ID) failed";

    return result;
}

//--------------------------------------------------------------------------------------------------
template <typename InputT, typename OutputT>
OutputT hashT(PasswordHash::Type type, const QString& password, InputT salt)
{
    switch (type)
    {
        case PasswordHash::SCRYPT:
            return hashScrypt<InputT, OutputT>(password, salt);

        case PasswordHash::ARGON2ID:
            return hashArgon2id<InputT, OutputT>(password, salt);

        default:
            NOTREACHED();
            return OutputT();
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QByteArray PasswordHash::hash(Type type, const QString& password, const QByteArray& salt)
{
    return hashT<const QByteArray, QByteArray>(type, password, salt);
}

} // namespace base
