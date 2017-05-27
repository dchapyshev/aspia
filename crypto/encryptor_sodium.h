//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor_sodium.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_SODIUM_H
#define _ASPIA_CRYPTO__ENCRYPTOR_SODIUM_H

#include "base/io_buffer.h"
#include "crypto/secure_buffer.h"

extern "C" {
#define SODIUM_STATIC
#include <sodium.h>
} // extern "C"

#include <memory>

namespace aspia {

class EncryptorSodium
{
public:
    ~EncryptorSodium();

    enum class Mode { SERVER, CLIENT };

    static std::unique_ptr<EncryptorSodium> Create(Mode mode);

    bool SetRemotePublicKey(const IOBuffer& public_key);
    IOBuffer GetLocalPublicKey();

    IOBuffer Encrypt(const IOBuffer& source_buffer);
    IOBuffer Decrypt(const IOBuffer& source_buffer);

private:
    EncryptorSodium(Mode mode, SecureBuffer public_key, SecureBuffer secret_key);

    const Mode mode_;

    SecureBuffer local_public_key_;
    SecureBuffer local_secret_key_;

    SecureBuffer encrypt_key_;
    SecureBuffer decrypt_key_;

    SecureBuffer nonce_;

    DISALLOW_COPY_AND_ASSIGN(EncryptorSodium);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__ENCRYPTOR_SODIUM_H
