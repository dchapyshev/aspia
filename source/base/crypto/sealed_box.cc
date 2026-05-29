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

#include "base/crypto/sealed_box.h"

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/secure_byte_array.h"

//--------------------------------------------------------------------------------------------------
// static
QByteArray SealedBox::seal(QByteArrayView plaintext, const QByteArray& recipient_public_key)
{
    if (recipient_public_key.size() != kPublicKeySize)
    {
        LOG(ERROR) << "Invalid recipient public key size:" << recipient_public_key.size();
        return QByteArray();
    }

    KeyPair ephemeral = KeyPair::create(KeyPair::Type::X25519);
    if (!ephemeral.isValid())
    {
        LOG(ERROR) << "Unable to generate ephemeral key pair";
        return QByteArray();
    }

    SecureByteArray shared_secret = ephemeral.sessionKey(recipient_public_key);
    if (shared_secret.isEmpty())
    {
        LOG(ERROR) << "Unable to derive shared secret";
        return QByteArray();
    }

    DataCryptor cryptor(CipherType::CHACHA20_POLY1305, shared_secret);
    std::optional<QByteArray> encrypted = cryptor.encrypt(plaintext);
    if (!encrypted.has_value())
    {
        LOG(ERROR) << "Unable to encrypt plaintext";
        return QByteArray();
    }

    return ephemeral.publicKey() + *encrypted;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QByteArray> SealedBox::open(QByteArrayView sealed, const KeyPair& recipient_key_pair)
{
    if (!recipient_key_pair.isValid())
    {
        LOG(ERROR) << "Invalid recipient key pair";
        return std::nullopt;
    }

    if (sealed.size() < kMinSealedSize)
    {
        LOG(ERROR) << "Sealed box too small:" << sealed.size();
        return std::nullopt;
    }

    const QByteArray ephemeral_public_key(sealed.first(kPublicKeySize).toByteArray());
    const QByteArrayView ciphertext = sealed.sliced(kPublicKeySize);

    SecureByteArray shared_secret = recipient_key_pair.sessionKey(ephemeral_public_key);
    if (shared_secret.isEmpty())
    {
        LOG(ERROR) << "Unable to derive shared secret";
        return std::nullopt;
    }

    DataCryptor cryptor(CipherType::CHACHA20_POLY1305, shared_secret);
    return cryptor.decrypt(ciphertext);
}
