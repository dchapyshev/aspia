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

#ifndef BASE_CRYPTO_SEALED_BOX_H
#define BASE_CRYPTO_SEALED_BOX_H

#include <QByteArray>
#include <QByteArrayView>

#include <optional>

#include "base/crypto/secure_byte_array.h"

class KeyPair;

// Anonymous public-key authenticated encryption.
//
// Sender knows only the recipient's public key. For each call seal() generates a fresh
// ephemeral X25519 keypair, derives a shared secret with recipient's public key and encrypts
// the message via AES256 GCM. The ephemeral public key is prepended to the ciphertext
// so the recipient can reconstruct the shared secret with their private key.
//
// The cipher key is BLAKE2s256(shared_secret || ephemeral_pk || recipient_pk), never the shared
// secret itself: the raw X25519 output is not key material, and hashing both public keys into the
// key ties a box to the pair it was sealed for.
//
// Output layout: ephemeral_pk (32) || iv (12) || ciphertext || tag (16).
class SealedBox
{
public:
    static constexpr int kPublicKeySize = 32;
    static constexpr int kMinSealedSize = kPublicKeySize + 12 + 16;

    static QByteArray seal(const SecureByteArray& plaintext, const QByteArray& recipient_public_key);
    static std::optional<SecureByteArray> open(QByteArrayView sealed, const KeyPair& recipient_key_pair);

private:
    Q_DISABLE_COPY_MOVE(SealedBox)
};

#endif // BASE_CRYPTO_SEALED_BOX_H
