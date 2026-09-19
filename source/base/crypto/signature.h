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

#ifndef BASE_CRYPTO_SIGNATURE_H
#define BASE_CRYPTO_SIGNATURE_H

#include <QByteArray>
#include <QByteArrayView>

class SecureByteArray;

// Ed25519 signatures. A key is 32 bytes and a signature is 64.
class Signature
{
public:
    // The public key that matches |private_key|. An empty array is returned when it could not be
    // derived.
    static QByteArray publicKey(const SecureByteArray& private_key);

    // Signs |data| with |private_key|. An empty array is returned when the signature could not be made.
    static QByteArray create(const SecureByteArray& private_key, const QByteArray& data);

    // True when |signature| is the one |public_key| makes for |data|.
    static bool verify(QByteArrayView public_key, QByteArrayView data, QByteArrayView signature);
};

#endif // BASE_CRYPTO_SIGNATURE_H
