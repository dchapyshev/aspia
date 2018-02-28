//
// PROJECT:         Aspia
// FILE:            crypto/encryptor.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

#include "crypto/secure_memory.h"
#include "protocol/io_buffer.h"

namespace aspia {

// Implements encryption of messages with using xsalsa20 + poly1305 algorithms.
class Encryptor
{
public:
    enum class Mode { SERVER, CLIENT };

    explicit Encryptor(Mode mode);
    ~Encryptor();

    bool ReadHelloMessage(const IOBuffer& message_buffer);
    IOBuffer HelloMessage();

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
