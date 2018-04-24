//
// PROJECT:         Aspia
// FILE:            crypto/encryptor.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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

    QByteArray public_key;
    public_key.resize(crypto_kx_PUBLICKEYBYTES);

    QByteArray secret_key;
    secret_key.resize(crypto_kx_SECRETKEYBYTES);

    if (crypto_kx_keypair(reinterpret_cast<quint8*>(public_key.data()),
                          reinterpret_cast<quint8*>(secret_key.data())) != 0)
    {
        qWarning("crypto_kx_keypair failed");
        return;
    }

    local_public_key_ = std::move(public_key);
    local_secret_key_ = std::move(secret_key);
}

Encryptor::~Encryptor()
{
    if (!encrypt_key_.isEmpty())
    {
        sodium_memzero(encrypt_key_.data(), encrypt_key_.size());
        encrypt_key_.clear();
    }

    if (!decrypt_key_.isEmpty())
    {
        sodium_memzero(decrypt_key_.data(), decrypt_key_.size());
        decrypt_key_.clear();
    }

    if (!encrypt_nonce_.isEmpty())
    {
        sodium_memzero(encrypt_nonce_.data(), encrypt_nonce_.size());
        encrypt_nonce_.clear();
    }

    if (!decrypt_nonce_.isEmpty())
    {
        sodium_memzero(decrypt_nonce_.data(), decrypt_nonce_.size());
        decrypt_nonce_.clear();
    }
}

bool Encryptor::readHelloMessage(const QByteArray& message_buffer)
{
    if (local_public_key_.isEmpty() || local_secret_key_.isEmpty())
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

    QByteArray decrypt_key;
    decrypt_key.resize(crypto_kx_SESSIONKEYBYTES);

    QByteArray encrypt_key;
    encrypt_key.resize(crypto_kx_SESSIONKEYBYTES);

    if (mode_ == ServerMode)
    {
        if (crypto_kx_server_session_keys(
                reinterpret_cast<quint8*>(decrypt_key.data()),
                reinterpret_cast<quint8*>(encrypt_key.data()),
                reinterpret_cast<const quint8*>(local_public_key_.data()),
                reinterpret_cast<const quint8*>(local_secret_key_.data()),
                reinterpret_cast<const quint8*>(message.public_key().data())) != 0)
        {
            qWarning("crypto_kx_server_session_keys failed");
            return false;
        }
    }
    else
    {
        Q_ASSERT(mode_ == ClientMode);

        if (crypto_kx_client_session_keys(
               reinterpret_cast<quint8*>(decrypt_key.data()),
               reinterpret_cast<quint8*>(encrypt_key.data()),
               reinterpret_cast<const quint8*>(local_public_key_.data()),
               reinterpret_cast<const quint8*>(local_secret_key_.data()),
               reinterpret_cast<const uint8_t*>(message.public_key().data())) != 0)
        {
            qWarning("crypto_kx_client_session_keys failed");
            return false;
        }
    }

    sodium_memzero(message.mutable_public_key()->data(), message.mutable_public_key()->size());
    sodium_memzero(message.mutable_nonce()->data(), message.mutable_nonce()->size());

    if (mode_ == ClientMode)
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
    if (local_public_key_.isEmpty() || local_secret_key_.isEmpty())
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

    if (mode_ == ServerMode)
    {
        sodium_memzero(local_public_key_.data(), local_public_key_.size());
        sodium_memzero(local_secret_key_.data(), local_secret_key_.size());

        local_public_key_.clear();
        local_secret_key_.clear();
    }

    return message_buffer;
}

QByteArray Encryptor::encrypt(const QByteArray& source_buffer)
{
    Q_ASSERT(local_public_key_.isEmpty());
    Q_ASSERT(local_secret_key_.isEmpty());
    Q_ASSERT(encrypt_nonce_.size() == crypto_secretbox_NONCEBYTES);
    Q_ASSERT(!encrypt_key_.isEmpty());

    sodium_increment(reinterpret_cast<quint8*>(encrypt_nonce_.data()),
                     crypto_secretbox_NONCEBYTES);

    QByteArray encrypted_buffer;
    encrypted_buffer.resize(source_buffer.size() + crypto_secretbox_MACBYTES);

    // Encrypt message.
    if (crypto_secretbox_easy(reinterpret_cast<quint8*>(encrypted_buffer.data()),
                              reinterpret_cast<const quint8*>(source_buffer.data()),
                              source_buffer.size(),
                              reinterpret_cast<const quint8*>(encrypt_nonce_.data()),
                              reinterpret_cast<const quint8*>(encrypt_key_.data())) != 0)
    {
        qWarning("crypto_secretbox_easy failed");
        return QByteArray();
    }

    return encrypted_buffer;
}

QByteArray Encryptor::decrypt(const QByteArray& source_buffer)
{
    Q_ASSERT(local_public_key_.isEmpty());
    Q_ASSERT(local_secret_key_.isEmpty());
    Q_ASSERT(decrypt_nonce_.size() == crypto_secretbox_NONCEBYTES);
    Q_ASSERT(!decrypt_key_.isEmpty());

    sodium_increment(reinterpret_cast<quint8*>(decrypt_nonce_.data()),
                     crypto_secretbox_NONCEBYTES);

    QByteArray decrypted_buffer;
    decrypted_buffer.resize(source_buffer.size() - crypto_secretbox_MACBYTES);

    // Decrypt message.
    if (crypto_secretbox_open_easy(reinterpret_cast<quint8*>(decrypted_buffer.data()),
                                   reinterpret_cast<const quint8*>(source_buffer.data()),
                                   source_buffer.size(),
                                   reinterpret_cast<const quint8*>(decrypt_nonce_.data()),
                                   reinterpret_cast<const quint8*>(decrypt_key_.data())) != 0)
    {
        qWarning("crypto_secretbox_open_easy failed");
        return QByteArray();
    }

    return decrypted_buffer;
}

} // namespace aspia
