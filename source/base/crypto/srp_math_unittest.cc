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

#include "base/crypto/srp_math.h"

#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"

#include <gtest/gtest.h>

namespace {

void runRoundTrip(const SrpMath::NgPair& Ng_pair)
{
    const QString I(QString("alice"));
    const SecureString p(QString("password123"));

    BigNum N = BigNum::fromStdString(Ng_pair.first);
    BigNum g = BigNum::fromStdString(Ng_pair.second);
    BigNum s = BigNum::fromByteArray(Random::byteArray(64));
    ASSERT_TRUE(N.isValid());
    ASSERT_TRUE(g.isValid());
    ASSERT_TRUE(s.isValid());

    BigNum x = SrpMath::calc_x(s, I, p);
    ASSERT_TRUE(x.isValid());

    BigNum v = SrpMath::calc_v(I, p, s, N, g);
    ASSERT_TRUE(v.isValid());

    BigNum a = BigNum::fromByteArray(Random::byteArray(32));
    BigNum b = BigNum::fromByteArray(Random::byteArray(32));
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(b.isValid());

    BigNum A = SrpMath::calc_A(a, N, g);
    ASSERT_TRUE(A.isValid());
    ASSERT_TRUE(SrpMath::verify_A_mod_N(A, N));

    BigNum B = SrpMath::calc_B(b, N, g, v);
    ASSERT_TRUE(B.isValid());
    ASSERT_TRUE(SrpMath::verify_B_mod_N(B, N));

    BigNum u = SrpMath::calc_u(A, B, N);
    ASSERT_TRUE(u.isValid());

    BigNum server_key = SrpMath::calcServerKey(A, v, u, b, N);
    ASSERT_TRUE(server_key.isValid());

    BigNum client_key = SrpMath::calcClientKey(N, B, g, x, a, u);
    ASSERT_TRUE(client_key.isValid());

    // The core protocol property: client and server, starting from password + salt and a single
    // round of A/B exchange, independently arrive at the same shared key.
    EXPECT_EQ(server_key.toByteArray(), client_key.toByteArray());
}

} // namespace

TEST(srp_math_test, round_trip_4096)
{
    runRoundTrip(SrpMath::kNgPair_4096);
}

TEST(srp_math_test, round_trip_6144)
{
    runRoundTrip(SrpMath::kNgPair_6144);
}

TEST(srp_math_test, round_trip_8192)
{
    runRoundTrip(SrpMath::kNgPair_8192);
}

TEST(srp_math_test, wrong_password_yields_different_key)
{
    const SrpMath::NgPair& Ng_pair = SrpMath::kNgPair_4096;
    const QString I(QString("alice"));

    BigNum N = BigNum::fromStdString(Ng_pair.first);
    BigNum g = BigNum::fromStdString(Ng_pair.second);
    BigNum s = BigNum::fromByteArray(Random::byteArray(64));

    const SecureString correct_password(QString("correct_password"));
    const SecureString wrong_password(QString("wrong_password"));

    BigNum v = SrpMath::calc_v(I, correct_password, s, N, g);

    BigNum a = BigNum::fromByteArray(Random::byteArray(32));
    BigNum b = BigNum::fromByteArray(Random::byteArray(32));

    BigNum A = SrpMath::calc_A(a, N, g);
    BigNum B = SrpMath::calc_B(b, N, g, v);
    BigNum u = SrpMath::calc_u(A, B, N);

    BigNum server_key = SrpMath::calcServerKey(A, v, u, b, N);

    // Client uses the wrong password.
    BigNum x_wrong = SrpMath::calc_x(s, I, wrong_password);
    BigNum client_key = SrpMath::calcClientKey(N, B, g, x_wrong, a, u);

    ASSERT_TRUE(server_key.isValid());
    ASSERT_TRUE(client_key.isValid());
    EXPECT_NE(server_key.toByteArray(), client_key.toByteArray());
}
