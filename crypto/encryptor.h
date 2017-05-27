//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

#include "base/io_buffer.h"
#include "crypto/secure_buffer.h"

extern "C" {
#define SODIUM_STATIC
#include <sodium.h>
} // extern "C"

#include <memory>

namespace aspia {

// Implements encryption of messages with using xsalsa20 + poly1305 algorithms.
class Encryptor
{
public:
    ~Encryptor();

    enum class Mode { SERVER, CLIENT };

    static std::unique_ptr<Encryptor> Create(Mode mode);

    bool ReadHelloMessage(const IOBuffer& message_buffer);
    IOBuffer HelloMessage();

    IOBuffer Encrypt(const IOBuffer& source_buffer);
    IOBuffer Decrypt(const IOBuffer& source_buffer);

private:
    Encryptor(Mode mode, SecureBuffer public_key, SecureBuffer secret_key);

    const Mode mode_;

    SecureBuffer local_public_key_;
    SecureBuffer local_secret_key_;

    SecureBuffer encrypt_key_;
    SecureBuffer decrypt_key_;

    SecureBuffer encrypt_nonce_;
    SecureBuffer decrypt_nonce_;

    DISALLOW_COPY_AND_ASSIGN(Encryptor);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__ENCRYPTOR_H
