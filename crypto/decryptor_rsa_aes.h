//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor_rsa_aes.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DECRYPTOR_RSA_AES_H
#define _ASPIA_CRYPTO__DECRYPTOR_RSA_AES_H

#include "aspia_config.h"

#include <wincrypt.h>

#include "base/macros.h"
#include "base/scoped_aligned_buffer.h"

#include "crypto/decryptor.h"

namespace aspia {

class DecryptorAES : public Decryptor
{
public:
    DecryptorAES();
    ~DecryptorAES();

    bool Init() override;
    uint32_t GetPublicKeySize() override;
    bool GetPublicKey(uint8_t* key, uint32_t len) override;
    bool SetSessionKey(const uint8_t* blob, uint32_t len) override;
    const uint8_t* Decrypt(const uint8_t* in, uint32_t in_len, uint32_t* out_len) override;

private:
    void Cleanup();

private:
    HCRYPTPROV prov_; // Контекст шифрования.

    HCRYPTKEY aes_key_; // Сессионный ключ для дешифрования.
    HCRYPTKEY rsa_key_;

    ScopedAlignedBuffer buffer_; // Буфер дешифрования.

    DISALLOW_COPY_AND_ASSIGN(DecryptorAES);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DECRYPTOR_RSA_AES_H
