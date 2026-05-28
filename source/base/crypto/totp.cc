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

#include "base/crypto/totp.h"

#include <openssl/evp.h>
#include <openssl/params.h>

#include <QUrl>
#include <QUrlQuery>

#include "base/logging.h"
#include "base/crypto/base32.h"
#include "base/crypto/random.h"

namespace {

//--------------------------------------------------------------------------------------------------
QByteArray hmacSha1(const QByteArray& key, const QByteArray& msg)
{
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    CHECK(mac) << "EVP_MAC_fetch(HMAC) failed";

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    CHECK(ctx) << "EVP_MAC_CTX_new failed";

    char digest_name[] = "SHA1";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", digest_name, 0),
        OSSL_PARAM_construct_end()
    };

    int ret = EVP_MAC_init(ctx,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           static_cast<size_t>(key.size()),
                           params);
    CHECK_EQ(ret, 1) << "EVP_MAC_init failed";

    ret = EVP_MAC_update(ctx,
                         reinterpret_cast<const unsigned char*>(msg.constData()),
                         static_cast<size_t>(msg.size()));
    CHECK_EQ(ret, 1) << "EVP_MAC_update failed";

    QByteArray out;
    out.resize(EVP_MAX_MD_SIZE);
    size_t out_len = 0;
    ret = EVP_MAC_final(ctx,
                        reinterpret_cast<unsigned char*>(out.data()),
                        &out_len,
                        static_cast<size_t>(out.size()));
    CHECK_EQ(ret, 1) << "EVP_MAC_final failed";

    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);

    out.resize(static_cast<qsizetype>(out_len));
    return out;
}

//--------------------------------------------------------------------------------------------------
quint32 truncateToCode(const QByteArray& hmac, int digits)
{
    DCHECK_GE(hmac.size(), 20);
    DCHECK_GT(digits, 0);
    DCHECK_LE(digits, 9);

    const quint8* bytes = reinterpret_cast<const quint8*>(hmac.constData());
    const int offset = bytes[hmac.size() - 1] & 0x0F;

    const quint32 binary = (static_cast<quint32>(bytes[offset]     & 0x7F) << 24)
                         | (static_cast<quint32>(bytes[offset + 1] & 0xFF) << 16)
                         | (static_cast<quint32>(bytes[offset + 2] & 0xFF) << 8)
                         | (static_cast<quint32>(bytes[offset + 3] & 0xFF));

    quint32 modulus = 1;
    for (int i = 0; i < digits; ++i)
        modulus *= 10;

    return binary % modulus;
}

//--------------------------------------------------------------------------------------------------
QString formatCode(quint32 value, int digits)
{
    return QString("%1").arg(value, digits, 10, QLatin1Char('0'));
}

//--------------------------------------------------------------------------------------------------
QByteArray counterBytes(quint64 counter)
{
    QByteArray out;
    out.resize(8);
    for (int i = 7; i >= 0; --i)
    {
        out[i] = static_cast<char>(counter & 0xFF);
        counter >>= 8;
    }
    return out;
}

//--------------------------------------------------------------------------------------------------
bool constantTimeEquals(const QString& a, const QString& b)
{
    if (a.size() != b.size())
        return false;

    quint32 diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        diff |= static_cast<quint32>(a[i].unicode() ^ b[i].unicode());
    return diff == 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QByteArray Totp::generateSecret(int bytes)
{
    CHECK_GE(bytes, 10);
    CHECK_LE(bytes, 64);
    return Random::byteArray(static_cast<size_t>(bytes));
}

//--------------------------------------------------------------------------------------------------
// static
QString Totp::hotp(const QByteArray& secret, quint64 counter, int digits)
{
    const QByteArray mac = hmacSha1(secret, counterBytes(counter));
    return formatCode(truncateToCode(mac, digits), digits);
}

//--------------------------------------------------------------------------------------------------
// static
QString Totp::code(const QByteArray& secret, qint64 unix_time_sec, int step_sec, int digits)
{
    CHECK_GT(step_sec, 0);
    const quint64 counter = static_cast<quint64>(unix_time_sec / step_sec);
    return hotp(secret, counter, digits);
}

//--------------------------------------------------------------------------------------------------
// static
bool Totp::verify(const QByteArray& secret, const QString& code, qint64 now_unix_sec, int step_sec,
    int digits, int window_steps, quint64* matched_counter)
{
    CHECK_GT(step_sec, 0);
    CHECK_GE(window_steps, 0);

    if (code.size() != digits)
        return false;

    const qint64 base_counter = now_unix_sec / step_sec;
    for (int delta = -window_steps; delta <= window_steps; ++delta)
    {
        const qint64 candidate_counter = base_counter + delta;
        if (candidate_counter < 0)
            continue;
        const QString candidate = hotp(secret, static_cast<quint64>(candidate_counter), digits);
        if (constantTimeEquals(candidate, code))
        {
            if (matched_counter)
                *matched_counter = static_cast<quint64>(candidate_counter);
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
// static
QString Totp::buildUri(const QString& issuer, const QString& account, const QByteArray& secret,
    int step_sec, int digits)
{
    const QString label = issuer.isEmpty() ? account : issuer + QLatin1Char(':') + account;

    QUrl url;
    url.setScheme("otpauth");
    url.setHost("totp");
    url.setPath(QLatin1Char('/') + label);

    QUrlQuery query;
    query.addQueryItem("secret", QString::fromLatin1(Base32::encode(secret, false)));
    if (!issuer.isEmpty())
        query.addQueryItem("issuer", issuer);
    query.addQueryItem("algorithm", "SHA1");
    query.addQueryItem("digits", QString::number(digits));
    query.addQueryItem("period", QString::number(step_sec));
    url.setQuery(query);

    return url.toString(QUrl::FullyEncoded);
}
