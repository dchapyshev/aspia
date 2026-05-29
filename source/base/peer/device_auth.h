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

#ifndef BASE_PEER_DEVICE_AUTH_H
#define BASE_PEER_DEVICE_AUTH_H

#include <QByteArray>

#include "base/crypto/secure_byte_array.h"

// Device-bound challenge-response used to re-authenticate a returning client without a fresh
// TOTP code.
//
// The client owns an Ed25519 key pair created per remote router; the public half is delivered
// to the router together with the token id at issuance and stored alongside it. On every
// subsequent connection the router sends a fresh |server_nonce|, the client signs the
// concatenation (token_id || server_nonce) and the router verifies the signature against the
// stored public key. Possession of just |token_id| - without the private key - is insufficient,
// which makes the token bound to the originating device.
//
// This class fixes the serialization format and exposes only the operations relevant to that
// flow. The underlying Ed25519 primitives are not exported, so callers cannot reuse the same
// keys for an unrelated signing scheme by accident.
class DeviceAuth
{
public:
    static const int kPrivateKeySize = 32;
    static const int kPublicKeySize = 32;
    static const int kSignatureSize = 64;
    static const int kServerNonceSize = 16;
    static const int kTokenIdSize = 16;

    struct DeviceKey
    {
        SecureByteArray private_key; // Ed25519 seed.
        QByteArray public_key;

        bool isValid() const
        {
            return private_key.size() == kPrivateKeySize && public_key.size() == kPublicKeySize;
        }
    };

    // Generates a fresh device key pair on the client side. Returns an invalid DeviceKey on
    // backend failure.
    static DeviceKey generateDeviceKey();

    // Derives the public half of |private_key| (used when restoring a key pair from the client
    // database, where only the private seed is persisted alongside the token id). Returns an
    // empty QByteArray on failure.
    static QByteArray derivePublicKey(const SecureByteArray& private_key);

    // Produces a fresh nonce suitable for use as the server-side challenge value.
    static QByteArray generateServerNonce();

    // Client side: signs (token_id || server_nonce) with the device private key. Returns an
    // empty QByteArray on any failure.
    static QByteArray signChallenge(const SecureByteArray& private_key,
        const QByteArray& token_id, const QByteArray& server_nonce);

    // Server side: verifies a signature produced by signChallenge() against the device public
    // key stored alongside the token. Returns false on malformed input or signature mismatch.
    static bool verifyChallenge(const QByteArray& public_key,
        const QByteArray& token_id, const QByteArray& server_nonce, const QByteArray& signature);

private:
    Q_DISABLE_COPY_MOVE(DeviceAuth)
};

#endif // BASE_PEER_DEVICE_AUTH_H
