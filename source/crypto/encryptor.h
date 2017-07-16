//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

#include "crypto/secure_buffer.h"
#include "crypto/secure_io_buffer.h"

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
    enum class Mode { SERVER, CLIENT };

    explicit Encryptor(Mode mode);
    ~Encryptor();

    bool ReadHelloMessage(const SecureIOBuffer& message_buffer);
    SecureIOBuffer HelloMessage();

    IOBuffer Encrypt(const IOBuffer& source_buffer);
    IOBuffer Decrypt(const IOBuffer& source_buffer);

private:
    const Mode mode_;

    std::unique_ptr<SecureBuffer> local_public_key_;
    std::unique_ptr<SecureBuffer> local_secret_key_;

    std::unique_ptr<SecureBuffer> encrypt_key_;
    std::unique_ptr<SecureBuffer> decrypt_key_;

    std::unique_ptr<SecureBuffer> encrypt_nonce_;
    std::unique_ptr<SecureBuffer> decrypt_nonce_;

    DISALLOW_COPY_AND_ASSIGN(Encryptor);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__ENCRYPTOR_H
