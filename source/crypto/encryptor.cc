//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/encryptor.h"
#include "crypto/secure_string.h"
#include "proto/key_exchange.pb.h"
#include "protocol/message_serialization.h"

namespace aspia {

Encryptor::Encryptor(Mode mode)
    : mode_(mode)
{
    if (sodium_init() == -1)
    {
        LOG(ERROR) << "sodium_init() failed";
        return;
    }

    std::unique_ptr<SecureBuffer> public_key =
        std::make_unique<SecureBuffer>(crypto_kx_PUBLICKEYBYTES);

    std::unique_ptr<SecureBuffer> secret_key =
        std::make_unique<SecureBuffer>(crypto_kx_SECRETKEYBYTES);

    if (crypto_kx_keypair(public_key->data(), secret_key->data()) != 0)
    {
        LOG(ERROR) << "crypto_kx_keypair() failed";
        return;
    }

    local_public_key_ = std::move(public_key);
    local_secret_key_ = std::move(secret_key);
}

Encryptor::~Encryptor()
{
    // Nothing
}

bool Encryptor::ReadHelloMessage(const IOBuffer& message_buffer)
{
    if (!local_public_key_ || !local_secret_key_)
        return false;

    proto::HelloMessage message;

    if (!ParseMessage(message_buffer, message))
        return false;

    if (message.public_key().size() != crypto_kx_PUBLICKEYBYTES)
        return false;

    if (message.nonce().size() != crypto_secretbox_NONCEBYTES)
        return false;

    decrypt_nonce_= std::make_unique<SecureBuffer>(crypto_secretbox_NONCEBYTES);
    memcpy(decrypt_nonce_->data(), message.nonce().data(), crypto_secretbox_NONCEBYTES);

    std::unique_ptr<SecureBuffer> decrypt_key =
        std::make_unique<SecureBuffer>(crypto_kx_SESSIONKEYBYTES);

    std::unique_ptr<SecureBuffer> encrypt_key =
        std::make_unique<SecureBuffer>(crypto_kx_SESSIONKEYBYTES);

    if (mode_ == Mode::SERVER)
    {
        if (crypto_kx_server_session_keys(decrypt_key->data(),
                                          encrypt_key->data(),
                                          local_public_key_->data(),
                                          local_secret_key_->data(),
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

        if (crypto_kx_client_session_keys(decrypt_key->data(),
                                          encrypt_key->data(),
                                          local_public_key_->data(),
                                          local_secret_key_->data(),
                                          reinterpret_cast<const uint8_t*>(
                                              message.public_key().data())) != 0)
        {
            LOG(ERROR) << "crypto_kx_client_session_keys() failed";
            return false;
        }
    }

    SecureClearString(*message.mutable_public_key());
    SecureClearString(*message.mutable_nonce());

    if (mode_ == Mode::CLIENT)
    {
        local_public_key_.reset();
        local_secret_key_.reset();
    }

    decrypt_key_ = std::move(decrypt_key);
    encrypt_key_ = std::move(encrypt_key);

    return true;
}

std::unique_ptr<IOBuffer> Encryptor::HelloMessage()
{
    if (!local_public_key_ || !local_secret_key_)
        return nullptr;

    encrypt_nonce_ = std::make_unique<SecureBuffer>(crypto_secretbox_NONCEBYTES);

    // Generate nonce for encryption.
    randombytes_buf(encrypt_nonce_->data(), encrypt_nonce_->size());

    proto::HelloMessage message;

    message.set_public_key(local_public_key_->data(), local_public_key_->size());
    message.set_nonce(encrypt_nonce_->data(), encrypt_nonce_->size());

    std::unique_ptr<IOBuffer> message_buffer = SerializeMessage<IOBuffer>(message);

    SecureClearString(*message.mutable_public_key());
    SecureClearString(*message.mutable_nonce());

    if (mode_ == Mode::SERVER)
    {
        local_public_key_.reset();
        local_secret_key_.reset();
    }

    return message_buffer;
}

std::unique_ptr<IOBuffer> Encryptor::Encrypt(const IOBuffer& source_buffer)
{
    DCHECK(!local_public_key_);
    DCHECK(!local_secret_key_);
    DCHECK(encrypt_nonce_);
    DCHECK_EQ(encrypt_nonce_->size(), crypto_secretbox_NONCEBYTES);
    DCHECK(encrypt_key_);

    sodium_increment(encrypt_nonce_->data(), crypto_secretbox_NONCEBYTES);

    std::unique_ptr<IOBuffer> encrypted_buffer =
        std::make_unique<IOBuffer>(source_buffer.size() + crypto_secretbox_MACBYTES);

    // Encrypt message.
    if (crypto_secretbox_easy(encrypted_buffer->data(),
                              source_buffer.data(),
                              source_buffer.size(),
                              encrypt_nonce_->data(),
                              encrypt_key_->data()) != 0)
    {
        LOG(ERROR) << "crypto_secretbox_easy() failed";
        return nullptr;
    }

    return encrypted_buffer;
}

std::unique_ptr<IOBuffer> Encryptor::Decrypt(const IOBuffer& source_buffer)
{
    DCHECK(!local_public_key_);
    DCHECK(!local_secret_key_);
    DCHECK(decrypt_nonce_);
    DCHECK_EQ(decrypt_nonce_->size(), crypto_secretbox_NONCEBYTES);
    DCHECK(decrypt_key_);

    sodium_increment(decrypt_nonce_->data(), crypto_secretbox_NONCEBYTES);

    std::unique_ptr<IOBuffer> decrypted_buffer =
        std::make_unique<IOBuffer>(source_buffer.size() - crypto_secretbox_MACBYTES);

    // Decrypt message.
    if (crypto_secretbox_open_easy(decrypted_buffer->data(),
                                   source_buffer.data(),
                                   source_buffer.size(),
                                   decrypt_nonce_->data(),
                                   decrypt_key_->data()) != 0)
    {
        LOG(ERROR) << "crypto_secretbox_open_easy() failed";
        return nullptr;
    }

    return decrypted_buffer;
}

} // namespace aspia
