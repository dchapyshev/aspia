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

#include "crypto/srp_math.h"

#include <openssl/opensslv.h>
#include <openssl/bn.h>

#include "base/logging.h"
#include "crypto/generic_hash.h"

namespace aspia {

namespace {

// xy = SHA1(PAD(x) || PAD(y))
BigNum calc_xy(const BigNum& x, const BigNum& y, const BigNum& N)
{
    if (x.get() != N.get() && BN_ucmp(x, N) >= 0)
        return BigNum();

    if (y.get() != N.get() && BN_ucmp(y, N) >= 0)
        return BigNum();

    int N_bytes = BN_num_bytes(N);
    if (N_bytes <= 0)
        return BigNum();

    std::unique_ptr<uint8_t[]> tmp_x = std::make_unique<uint8_t[]>(N_bytes);
    if (BN_bn2binpad(x, tmp_x.get(), N_bytes) < 0)
        return BigNum();

    std::unique_ptr<uint8_t[]> tmp_y = std::make_unique<uint8_t[]>(N_bytes);
    if (BN_bn2binpad(y, tmp_y.get(), N_bytes) < 0)
        return BigNum();

    GenericHash hash(GenericHash::SHA1);

    hash.addData(tmp_x.get(), N_bytes);
    hash.addData(tmp_y.get(), N_bytes);

    return BigNum::fromStdString(hash.result());
}

// k = SHA1(N | PAD(g))
BigNum calc_k(const BigNum& N, const BigNum& g)
{
    return calc_xy(N, g, N);
}

} // namespace

// static
// u = SHA1(PAD(A) | PAD(B))
BigNum SrpMath::calc_u(const BigNum& A, const BigNum& B, const BigNum& N)
{
    return calc_xy(A, B, N);
}

// static
// B = k*v + g^b % N
BigNum SrpMath::calc_B(const BigNum& b, const BigNum& N, const BigNum& g, const BigNum& v)
{
    if (!b.isValid() || !N.isValid() || !g.isValid() || !v.isValid())
        return BigNum();

    BigNum::Context ctx = BigNum::Context::create();
    if (!ctx.isValid())
        return BigNum();

    BigNum gb = BigNum::create();
    if (!gb.isValid())
        return BigNum();

    if (!BN_mod_exp(gb, g, b, N, ctx))
        return BigNum();

    BigNum k = calc_k(N, g);
    if (!k.isValid())
        return BigNum();

    BigNum kv = BigNum::create();
    if (!kv.isValid())
        return BigNum();

    if (!BN_mod_mul(kv, v, k, N, ctx))
        return BigNum();

    BigNum B = BigNum::create();
    if (!B.isValid())
        return BigNum();

    if (!BN_mod_add(B, gb, kv, N, ctx))
        return BigNum();

    return B;
}

// static
// x = SHA1(s | SHA1(I | ":" | p))
BigNum SrpMath::calc_x(const BigNum& s, const std::string& I, const std::string& p)
{
    if (!s.isValid() || I.empty() || p.empty())
        return BigNum();

    GenericHash hash(GenericHash::SHA1);

    hash.addData(I);
    hash.addData(":");
    hash.addData(p);

    GenericHash result_hash(GenericHash::SHA1);

    result_hash.addData(s.toStdString());
    result_hash.addData(hash.result());

    return BigNum::fromStdString(result_hash.result());
}

// static
// A = g^a % N
BigNum SrpMath::calc_A(const BigNum& a, const BigNum& N, const BigNum& g)
{
    if (!a.isValid() || !N.isValid() || !g.isValid())
        return BigNum();

    BigNum::Context ctx = BigNum::Context::create();
    BigNum A = BigNum::create();

    if (!A.isValid() || !ctx.isValid())
        return BigNum();

    if (!BN_mod_exp(A, g, a, N, ctx))
        return BigNum();

    return A;
}

// static
// S = (A * v^u) ^ b % N
BigNum SrpMath::calcServerKey(const BigNum& A, const BigNum& v, const BigNum& u, const BigNum& b,
                              const BigNum& N)
{
    if (!A.isValid() || !v.isValid() || !u.isValid() || !b.isValid() || !N.isValid())
    {
        DLOG(LS_WARNING) << "Invalid parameters";
        return BigNum();
    }

    BigNum::Context ctx = BigNum::Context::create();
    BigNum tmp = BigNum::create();

    if (!ctx.isValid() || !tmp.isValid())
        return BigNum();

    if (!BN_mod_exp(tmp, v, u, N, ctx))
        return BigNum();

    if (!BN_mod_mul(tmp, A, tmp, N, ctx))
        return BigNum();

    BigNum S = BigNum::create();
    if (!S.isValid())
        return BigNum();

    if (!BN_mod_exp(S, tmp, b, N, ctx))
        return BigNum();

    return S;
}

// static
// K = (B - (k * g^x)) ^ (a + (u * x)) % N
BigNum SrpMath::calcClientKey(const BigNum& N, const BigNum& B, const BigNum& g, const BigNum& x,
                              const BigNum& a, const BigNum& u)
{
    if (!N.isValid() || !B.isValid() || !g.isValid() || !x.isValid() || !a.isValid() || !u.isValid())
        return BigNum();

    BigNum::Context ctx = BigNum::Context::create();
    if (!ctx.isValid())
        return BigNum();

    BigNum tmp = BigNum::create();
    BigNum tmp2 = BigNum::create();
    BigNum tmp3 = BigNum::create();

    if (!tmp.isValid() || !tmp2.isValid() || !tmp3.isValid())
        return BigNum();

    if (!BN_mod_exp(tmp, g, x, N, ctx))
        return BigNum();

    BigNum k = calc_k(N, g);
    if (!k.isValid())
        return BigNum();

    if (!BN_mod_mul(tmp2, tmp, k, N, ctx))
        return BigNum();

    if (!BN_mod_sub(tmp, B, tmp2, N, ctx))
        return BigNum();

    if (!BN_mul(tmp3, u, x, ctx))
        return BigNum();

    if (!BN_add(tmp2, a, tmp3))
        return BigNum();

    BigNum K = BigNum::create();
    if (!K.isValid())
        return BigNum();

    if (!BN_mod_exp(K, tmp, tmp2, N, ctx))
        return BigNum();

    return K;
}

// static
bool SrpMath::verify_B_mod_N(const BigNum& B, const BigNum& N)
{
    if (!B.isValid() || !N.isValid())
        return false;

    BigNum::Context ctx = BigNum::Context::create();
    BigNum result = BigNum::create();

    if (!ctx.isValid() || !result.isValid())
        return false;

    if (!BN_nnmod(result, B, N, ctx))
        return false;

    return !BN_is_zero(result);
}

// static
bool SrpMath::verify_A_mod_N(const BigNum& A, const BigNum& N)
{
    return verify_B_mod_N(A, N);
}

// static
BigNum SrpMath::calc_v(const std::string& I, const std::string& p, const BigNum& s,
                       const BigNum& N, const BigNum& g)
{
    if (I.empty() || p.empty() || !N.isValid() || !g.isValid() || !s.isValid())
        return BigNum();

    BigNum::Context ctx = BigNum::Context::create();
    BigNum v = BigNum::create();

    if (!ctx.isValid() || !v.isValid())
        return BigNum();

    BigNum x = calc_x(s, I, p);

    if (!BN_mod_exp(v, g, x, N, ctx))
        return BigNum();

    return v;
}

} // namespace aspia
