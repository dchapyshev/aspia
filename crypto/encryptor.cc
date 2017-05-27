//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/encryptor.h"
#include "base/logging.h"
#include "proto/key_exchange.pb.h"
#include "protocol/message_serialization.h"

namespace aspia {

Encryptor::Encryptor(Mode mode, SecureBuffer public_key, SecureBuffer secret_key) :
    local_public_key_(std::move(public_key)),
    local_secret_key_(std::move(secret_key)),
    decrypt_key_(crypto_kx_SESSIONKEYBYTES),
    encrypt_key_(crypto_kx_SESSIONKEYBYTES),
    encrypt_nonce_(crypto_secretbox_NONCEBYTES),
    decrypt_nonce_(crypto_secretbox_NONCEBYTES),
    mode_(mode)
{
    // Generate nonce for encryption.
    randombytes_buf(encrypt_nonce_.data(), encrypt_nonce_.size());
}

Encryptor::~Encryptor()
{
    // Nothing
}

// static
std::unique_ptr<Encryptor> Encryptor::Create(Mode mode)
{
    if (sodium_init() == -1)
    {
        LOG(ERROR) << "sodium_init() failed";
        return nullptr;
    }

    SecureBuffer public_key(crypto_kx_PUBLICKEYBYTES);
    SecureBuffer secret_key(crypto_kx_SECRETKEYBYTES);

    if (crypto_kx_keypair(public_key.data(), secret_key.data()) != 0)
    {
        LOG(ERROR) << "crypto_kx_keypair() failed";
        return nullptr;
    }

    return std::unique_ptr<Encryptor>(new Encryptor(mode,
                                                    std::move(public_key),
                                                    std::move(secret_key)));
}

bool Encryptor::ReadHelloMessage(const IOBuffer& message_buffer)
{
    DCHECK_EQ(decrypt_nonce_.size(), crypto_secretbox_NONCEBYTES);

    proto::HelloMessage message;

    if (!ParseMessage(message_buffer, message))
        return false;

    if (message.public_key().size() != crypto_kx_PUBLICKEYBYTES)
        return false;

    if (message.nonce().size() != crypto_secretbox_NONCEBYTES)
        return false;

    memcpy(decrypt_nonce_.data(), message.nonce().data(), decrypt_nonce_.size());

    if (mode_ == Mode::SERVER)
    {
        if (crypto_kx_server_session_keys(decrypt_key_.data(),
                                          encrypt_key_.data(),
                                          local_public_key_.data(),
                                          local_secret_key_.data(),
                                          reinterpret_cast<const uint8_t*>(
                                              message.public_key().data())) != 0)
        {
            LOG(ERROR) << "crypto_kx_server_session_keys() failed";
            return false;
        }
    }
    else
    {
        DCHECK(mode_ == Mode::CLIENT);

        if (crypto_kx_client_session_keys(decrypt_key_.data(),
                                          encrypt_key_.data(),
                                          local_public_key_.data(),
                                          local_secret_key_.data(),
                                          reinterpret_cast<const uint8_t*>(
                                              message.public_key().data())) != 0)
        {
            LOG(ERROR) << "crypto_kx_client_session_keys() failed";
            return false;
        }
    }

    sodium_memzero(message.mutable_public_key(), message.public_key().size());
    sodium_memzero(message.mutable_nonce(), message.nonce().size());

    return true;
}

IOBuffer Encryptor::HelloMessage()
{
    if (local_public_key_.IsEmpty() || encrypt_nonce_.IsEmpty())
        return IOBuffer();

    proto::HelloMessage message;

    message.set_public_key(local_public_key_.data(), local_public_key_.size());
    message.set_nonce(encrypt_nonce_.data(), encrypt_nonce_.size());

    IOBuffer message_buffer(SerializeMessage(message));

    sodium_memzero(message.mutable_public_key(), message.public_key().size());
    sodium_memzero(message.mutable_nonce(), message.nonce().size());

    return message_buffer;
}

IOBuffer Encryptor::Encrypt(const IOBuffer& source_buffer)
{
    DCHECK_EQ(encrypt_nonce_.size(), crypto_secretbox_NONCEBYTES);

    if (source_buffer.IsEmpty())
        return IOBuffer();

    sodium_increment(encrypt_nonce_.data(), crypto_secretbox_NONCEBYTES);

    IOBuffer encrypted_buffer(source_buffer.size() + crypto_secretbox_MACBYTES);

    // Encrypt message.
    if (crypto_secretbox_easy(encrypted_buffer.data(),
                              source_buffer.data(),
                              source_buffer.size(),
                              encrypt_nonce_.data(),
                              encrypt_key_.data()) != 0)
    {
        LOG(ERROR) << "crypto_secretbox_easy() failed";
        return IOBuffer();
    }

    return encrypted_buffer;
}

IOBuffer Encryptor::Decrypt(const IOBuffer& source_buffer)
{
    DCHECK_EQ(decrypt_nonce_.size(), crypto_secretbox_NONCEBYTES);

    if (source_buffer.IsEmpty())
        return IOBuffer();

    sodium_increment(decrypt_nonce_.data(), crypto_secretbox_NONCEBYTES);

    IOBuffer decrypted_buffer(source_buffer.size() - crypto_secretbox_MACBYTES);

    // Decrypt message.
    if (crypto_secretbox_open_easy(decrypted_buffer.data(),
                                   source_buffer.data(),
                                   source_buffer.size(),
                                   decrypt_nonce_.data(),
                                   decrypt_key_.data()) != 0)
    {
        LOG(ERROR) << "crypto_secretbox_open_easy() failed";
        return IOBuffer();
    }

    return decrypted_buffer;
}

} // namespace aspia
