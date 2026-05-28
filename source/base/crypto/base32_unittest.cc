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

#include "base/crypto/base32.h"

#include <gtest/gtest.h>

namespace {

struct Vector
{
    const char* plain;
    const char* encoded;
};

// RFC 4648 section 10 Base32 test vectors.
const Vector kRfc4648Vectors[] = {
    { "",       ""                 },
    { "f",      "MY======"         },
    { "fo",     "MZXQ===="         },
    { "foo",    "MZXW6==="         },
    { "foob",   "MZXW6YQ="         },
    { "fooba",  "MZXW6YTB"         },
    { "foobar", "MZXW6YTBOI======" }
};

} // namespace

TEST(Base32Test, EncodeRfc4648)
{
    for (const Vector& v : kRfc4648Vectors)
    {
        QByteArray expected = QByteArray::fromRawData(v.encoded, qstrlen(v.encoded));
        QByteArray got = Base32::encode(QByteArray::fromRawData(v.plain, qstrlen(v.plain)), true);
        EXPECT_EQ(got, expected) << "input: '" << v.plain << "'";
    }
}

TEST(Base32Test, EncodeWithoutPadding)
{
    EXPECT_EQ(Base32::encode("f", false), QByteArray("MY"));
    EXPECT_EQ(Base32::encode("fo", false), QByteArray("MZXQ"));
    EXPECT_EQ(Base32::encode("foo", false), QByteArray("MZXW6"));
    EXPECT_EQ(Base32::encode("foob", false), QByteArray("MZXW6YQ"));
    EXPECT_EQ(Base32::encode("fooba", false), QByteArray("MZXW6YTB"));
    EXPECT_EQ(Base32::encode("foobar", false), QByteArray("MZXW6YTBOI"));
}

TEST(Base32Test, DecodeRfc4648)
{
    for (const Vector& v : kRfc4648Vectors)
    {
        bool ok = false;
        QByteArray got = Base32::decode(QByteArray::fromRawData(v.encoded, qstrlen(v.encoded)), &ok);
        EXPECT_TRUE(ok) << "input: '" << v.encoded << "'";
        EXPECT_EQ(got, QByteArray::fromRawData(v.plain, qstrlen(v.plain)));
    }
}

TEST(Base32Test, DecodeWithoutPadding)
{
    bool ok = false;
    EXPECT_EQ(Base32::decode("MZXW6YTBOI", &ok), QByteArray("foobar"));
    EXPECT_TRUE(ok);
    EXPECT_EQ(Base32::decode("MZXW6", &ok), QByteArray("foo"));
    EXPECT_TRUE(ok);
}

TEST(Base32Test, DecodeAcceptsLowercase)
{
    bool ok = false;
    EXPECT_EQ(Base32::decode("mzxw6ytboi", &ok), QByteArray("foobar"));
    EXPECT_TRUE(ok);
}

TEST(Base32Test, DecodeRejectsInvalidCharacter)
{
    bool ok = true;
    EXPECT_EQ(Base32::decode("MZXW6YTB1I", &ok), QByteArray());
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_EQ(Base32::decode("MZXW6YTB!", &ok), QByteArray());
    EXPECT_FALSE(ok);
}

TEST(Base32Test, DecodeRejectsIllegalLength)
{
    // Per RFC 4648 section 6, a final group of 1, 3 or 6 unpadded characters is not legal.
    bool ok = true;
    EXPECT_EQ(Base32::decode("M", &ok), QByteArray());
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_EQ(Base32::decode("MZX", &ok), QByteArray());
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_EQ(Base32::decode("MZXW6Y", &ok), QByteArray());
    EXPECT_FALSE(ok);
}

TEST(Base32Test, RoundTripRandomBytes)
{
    for (int len = 1; len <= 64; ++len)
    {
        QByteArray input(len, Qt::Uninitialized);
        for (int i = 0; i < len; ++i)
            input[i] = static_cast<char>((i * 31 + 7) & 0xFF);

        bool ok = false;
        QByteArray decoded = Base32::decode(Base32::encode(input, true), &ok);
        EXPECT_TRUE(ok) << "len=" << len;
        EXPECT_EQ(decoded, input) << "len=" << len;

        decoded = Base32::decode(Base32::encode(input, false), &ok);
        EXPECT_TRUE(ok) << "len=" << len << " (unpadded)";
        EXPECT_EQ(decoded, input) << "len=" << len << " (unpadded)";
    }
}
