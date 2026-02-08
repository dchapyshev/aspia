//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CRYPTO_SRP_MATH_H
#define BASE_CRYPTO_SRP_MATH_H

#include <QByteArray>
#include <QString>

#include "base/crypto/big_num.h"

namespace base {

// This class implements primitives to perform authorization using SRP algorithm.
class SrpMath
{
public:
    // N - big prime number (N = 2q + 1, where q prime also).
    // k - multiplier parameter.
    // s - user-specified cryptographic random parameter for hashing.
    // I - unique user identifier (user name).
    // p - plain-text password.
    // u - masked value of protocol SRP.
    // a, b - secret random values.
    // A, B - ephemeral keys.
    // x - private key.
    // v - password-proof value.
    // g - generator of prime modulo N.

    static BigNum calc_u(const BigNum& A, const BigNum& B, const BigNum& N);
    static BigNum calc_B(const BigNum& b, const BigNum& N, const BigNum& g, const BigNum& v);
    static BigNum calc_x(const BigNum& s, const QString& I, const QString& p);
    static BigNum calc_x(const BigNum& s, const QString& I, const QByteArray& p);
    static BigNum calc_A(const BigNum& a, const BigNum& N, const BigNum& g);

    static BigNum calcServerKey(const BigNum& A, const BigNum& v, const BigNum& u, const BigNum& b,
                                const BigNum& N);

    static BigNum calcClientKey(const BigNum& N, const BigNum& B, const BigNum& g, const BigNum& x,
                                const BigNum& a, const BigNum& u);

    // Checks if B % N == 0.
    static bool verify_B_mod_N(const BigNum& B, const BigNum& N);

    // Checks if A % N == 0.
    static bool verify_A_mod_N(const BigNum& A, const BigNum& N);

    static BigNum calc_v(const QString& I, const QString& p, const BigNum& s,
                         const BigNum& N, const BigNum& g);

    static BigNum calc_v(const QString& I, const QByteArray& p, const BigNum& s,
                         const BigNum& N, const BigNum& g);

private:
    Q_DISABLE_COPY(SrpMath)
};

} // namespace base

#endif // BASE_CRYPTO_SRP_MATH_H
