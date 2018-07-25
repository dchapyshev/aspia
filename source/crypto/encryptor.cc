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

#include "crypto/encryptor.h"

#include "base/message_serialization.h"
#include "protocol/key_exchange.pb.h"

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

namespace aspia {

Encryptor::Encryptor(Mode mode)
    : mode_(mode)
{
    if (sodium_init() == -1)
    {
        qWarning("sodium_init failed");
        return;
    }

    std::vector<uint8_t> public_key;
    public_key.resize(crypto_kx_PUBLICKEYBYTES);

    std::vector<uint8_t> secret_key;
    secret_key.resize(crypto_kx_SECRETKEYBYTES);

    if (crypto_kx_keypair(public_key.data(), secret_key.data()) != 0)
    {
        qWarning("crypto_kx_keypair failed");
        return;
    }

    local_public_key_ = std::move(public_key);
    local_secret_key_ = std::move(secret_key);
}

Encryptor::~Encryptor()
{
    if (!encrypt_key_.empty())
    {
        sodium_memzero(encrypt_key_.data(), encrypt_key_.size());
        encrypt_key_.clear();
    }

    if (!decrypt_key_.empty())
    {
        sodium_memzero(decrypt_key_.data(), decrypt_key_.size());
        decrypt_key_.clear();
    }

    if (!encrypt_nonce_.empty())
    {
        sodium_memzero(encrypt_nonce_.data(), encrypt_nonce_.size());
        encrypt_nonce_.clear();
    }

    if (!decrypt_nonce_.empty())
    {
        sodium_memzero(decrypt_nonce_.data(), decrypt_nonce_.size());
        decrypt_nonce_.clear();
    }
}

bool Encryptor::readHelloMessage(const QByteArray& message_buffer)
{
    if (local_public_key_.empty() || local_secret_key_.empty())
        return false;

    proto::HelloMessage message;

    if (!parseMessage(message_buffer, message))
        return false;

    if (message.public_key().size() != crypto_kx_PUBLICKEYBYTES)
        return false;

    if (message.nonce().size() != crypto_secretbox_NONCEBYTES)
        return false;

    decrypt_nonce_.resize(crypto_secretbox_NONCEBYTES);
    memcpy(decrypt_nonce_.data(), message.nonce().data(), crypto_secretbox_NONCEBYTES);

    std::vector<uint8_t> decrypt_key;
    decrypt_key.resize(crypto_kx_SESSIONKEYBYTES);

    std::vector<uint8_t> encrypt_key;
    encrypt_key.resize(crypto_kx_SESSIONKEYBYTES);

    if (mode_ == Mode::SERVER)
    {
        if (crypto_kx_server_session_keys(
                decrypt_key.data(),
                encrypt_key.data(),
                local_public_key_.data(),
                local_secret_key_.data(),
                reinterpret_cast<const uint8_t*>(message.public_key().data())) != 0)
        {
            qWarning("crypto_kx_server_session_keys failed");
            return false;
        }
    }
    else
    {
        Q_ASSERT(mode_ == Mode::CLIENT);

        if (crypto_kx_client_session_keys(
               decrypt_key.data(),
               encrypt_key.data(),
               local_public_key_.data(),
               local_secret_key_.data(),
               reinterpret_cast<const uint8_t*>(message.public_key().data())) != 0)
        {
            qWarning("crypto_kx_client_session_keys failed");
            return false;
        }
    }

    sodium_memzero(message.mutable_public_key()->data(), message.mutable_public_key()->size());
    sodium_memzero(message.mutable_nonce()->data(), message.mutable_nonce()->size());

    if (mode_ == Mode::CLIENT)
    {
        sodium_memzero(local_public_key_.data(), local_public_key_.size());
        sodium_memzero(local_secret_key_.data(), local_secret_key_.size());

        local_public_key_.clear();
        local_secret_key_.clear();
    }

    decrypt_key_ = std::move(decrypt_key);
    encrypt_key_ = std::move(encrypt_key);

    return true;
}

QByteArray Encryptor::helloMessage()
{
    if (local_public_key_.empty() || local_secret_key_.empty())
        return QByteArray();

    encrypt_nonce_.resize(crypto_secretbox_NONCEBYTES);

    // Generate nonce for encryption.
    randombytes_buf(encrypt_nonce_.data(), encrypt_nonce_.size());

    proto::HelloMessage message;

    message.set_public_key(local_public_key_.data(), local_public_key_.size());
    message.set_nonce(encrypt_nonce_.data(), encrypt_nonce_.size());

    QByteArray message_buffer = serializeMessage(message);

    sodium_memzero(message.mutable_public_key()->data(), message.mutable_public_key()->size());
    sodium_memzero(message.mutable_nonce()->data(), message.mutable_nonce()->size());

    if (mode_ == Mode::SERVER)
    {
        sodium_memzero(local_public_key_.data(), local_public_key_.size());
        sodium_memzero(local_secret_key_.data(), local_secret_key_.size());

        local_public_key_.clear();
        local_secret_key_.clear();
    }

    return message_buffer;
}

int Encryptor::encryptedDataSize(int source_data_size)
{
    return source_data_size + crypto_secretbox_MACBYTES;
}

bool Encryptor::encrypt(const char* source, int source_size, char* target)
{
    Q_ASSERT(local_public_key_.empty());
    Q_ASSERT(local_secret_key_.empty());
    Q_ASSERT(encrypt_nonce_.size() == crypto_secretbox_NONCEBYTES);
    Q_ASSERT(!encrypt_key_.empty());

    sodium_increment(encrypt_nonce_.data(), crypto_secretbox_NONCEBYTES);

    // Encrypt message.
    if (crypto_secretbox_easy(reinterpret_cast<uint8_t*>(target),
                              reinterpret_cast<const uint8_t*>(source),
                              source_size,
                              encrypt_nonce_.data(),
                              encrypt_key_.data()) != 0)
    {
        qWarning("crypto_secretbox_easy failed");
        return false;
    }

    return true;
}

int Encryptor::decryptedDataSize(int source_data_size)
{
    return source_data_size - crypto_secretbox_MACBYTES;
}

bool Encryptor::decrypt(const char* source, int source_size, char* target)
{
    Q_ASSERT(local_public_key_.empty());
    Q_ASSERT(local_secret_key_.empty());
    Q_ASSERT(decrypt_nonce_.size() == crypto_secretbox_NONCEBYTES);
    Q_ASSERT(!decrypt_key_.empty());

    sodium_increment(decrypt_nonce_.data(), crypto_secretbox_NONCEBYTES);

    // Decrypt message.
    if (crypto_secretbox_open_easy(reinterpret_cast<uint8_t*>(target),
                                   reinterpret_cast<const uint8_t*>(source),
                                   source_size,
                                   decrypt_nonce_.data(),
                                   decrypt_key_.data()) != 0)
    {
        qWarning("crypto_secretbox_open_easy failed");
        return false;
    }

    return true;
}

} // namespace aspia
