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

#include <gtest/gtest.h>

#include <QUrl>
#include <QUrlQuery>

#include "base/crypto/base32.h"

namespace {

// Shared secret used by both RFC 4226 appendix D and RFC 6238 appendix B:
// ASCII "12345678901234567890".
const QByteArray& rfcSecret()
{
    static const QByteArray secret = QByteArrayLiteral("12345678901234567890");
    return secret;
}

} // namespace

// RFC 4226 Appendix D - HOTP test vectors with the 20-byte ASCII secret.
TEST(TotpTest, HotpRfc4226)
{
    struct Vector { quint64 counter; const char* expected; };
    const Vector kVectors[] = {
        { 0, "755224" }, { 1, "287082" }, { 2, "359152" }, { 3, "969429" },
        { 4, "338314" }, { 5, "254676" }, { 6, "287922" }, { 7, "162583" },
        { 8, "399871" }, { 9, "520489" }
    };

    for (const Vector& v : kVectors)
    {
        EXPECT_EQ(Totp::hotp(rfcSecret(), v.counter, 6), QString(v.expected))
            << "counter=" << v.counter;
    }
}

// RFC 6238 Appendix B - TOTP test vectors for the SHA-1 variant (truncated to 6 digits).
TEST(TotpTest, TotpRfc6238)
{
    struct Vector { qint64 time_sec; const char* expected; };
    const Vector kVectors[] = {
        {          59, "287082" },
        {  1111111109, "081804" },
        {  1111111111, "050471" },
        {  1234567890, "005924" },
        {  2000000000, "279037" }
    };

    for (const Vector& v : kVectors)
    {
        EXPECT_EQ(Totp::code(rfcSecret(), v.time_sec, 30, 6), QString(v.expected))
            << "t=" << v.time_sec;
    }
}

// RFC 6238 Appendix B truncated to 8 digits (additional sanity-check).
TEST(TotpTest, TotpRfc6238EightDigits)
{
    struct Vector { qint64 time_sec; const char* expected; };
    const Vector kVectors[] = {
        {          59, "94287082" },
        {  1111111109, "07081804" },
        {  1111111111, "14050471" },
        {  1234567890, "89005924" },
        {  2000000000, "69279037" }
    };

    for (const Vector& v : kVectors)
    {
        EXPECT_EQ(Totp::code(rfcSecret(), v.time_sec, 30, 8), QString(v.expected))
            << "t=" << v.time_sec;
    }
}

TEST(TotpTest, VerifyAcceptsCurrentStep)
{
    const qint64 now = 1111111111; // counter = 37037037
    const QString good = Totp::code(rfcSecret(), now);

    quint64 matched = 0;
    EXPECT_TRUE(Totp::verify(rfcSecret(), good, now, 30, 6, 1, &matched));
    EXPECT_EQ(matched, 37037037u);
}

TEST(TotpTest, VerifyAcceptsAdjacentStepsWithinWindow)
{
    const qint64 now = 1111111111;
    const QString prev = Totp::code(rfcSecret(), now - 30);
    const QString next = Totp::code(rfcSecret(), now + 30);

    EXPECT_TRUE(Totp::verify(rfcSecret(), prev, now, 30, 6, 1));
    EXPECT_TRUE(Totp::verify(rfcSecret(), next, now, 30, 6, 1));
}

TEST(TotpTest, VerifyRejectsCodesOutsideWindow)
{
    const qint64 now = 1111111111;
    const QString far_past = Totp::code(rfcSecret(), now - 600);
    EXPECT_FALSE(Totp::verify(rfcSecret(), far_past, now, 30, 6, 1));
}

TEST(TotpTest, VerifyRejectsWrongLength)
{
    const qint64 now = 1111111111;
    EXPECT_FALSE(Totp::verify(rfcSecret(), "12345", now));
    EXPECT_FALSE(Totp::verify(rfcSecret(), "1234567", now));
}

TEST(TotpTest, GenerateSecretHasRequestedLength)
{
    EXPECT_EQ(Totp::generateSecret(20).size(), 20);
    EXPECT_EQ(Totp::generateSecret(16).size(), 16);
}

TEST(TotpTest, GeneratedSecretsDiffer)
{
    QByteArray a = Totp::generateSecret(20);
    QByteArray b = Totp::generateSecret(20);
    EXPECT_NE(a, b);
}

TEST(TotpTest, BuildUriContainsExpectedFields)
{
    const QByteArray secret = QByteArrayLiteral("Hello!\xDE\xAD\xBE\xEF");
    const QString uri = Totp::buildUri("Aspia", "user@host", secret);

    const QUrl url(uri);
    EXPECT_EQ(url.scheme(), "otpauth");
    EXPECT_EQ(url.host(), "totp");
    EXPECT_EQ(url.path(), "/Aspia:user@host");

    const QUrlQuery query(url.query());
    EXPECT_EQ(query.queryItemValue("issuer"), "Aspia");
    EXPECT_EQ(query.queryItemValue("algorithm"), "SHA1");
    EXPECT_EQ(query.queryItemValue("digits"), "6");
    EXPECT_EQ(query.queryItemValue("period"), "30");

    const QString b32_secret = query.queryItemValue("secret");
    bool ok = false;
    EXPECT_EQ(Base32::decode(b32_secret.toLatin1(), &ok), secret);
    EXPECT_TRUE(ok);
    EXPECT_FALSE(b32_secret.contains(QLatin1Char('=')));
}
