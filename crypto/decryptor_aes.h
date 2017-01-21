//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor_aes.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DECRYPTOR_AES_H
#define _ASPIA_CRYPTO__DECRYPTOR_AES_H

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

    uint32_t GetPublicKeySize() override;
    void GetPublicKey(uint8_t *key, uint32_t len) override;
    void SetSessionKey(const uint8_t *blob, uint32_t len) override;

    void Decrypt(const uint8_t *in, uint32_t in_len,
                 uint8_t **out, uint32_t *out_len) override;

private:
    void Cleanup();

private:
    HCRYPTPROV prov_; // Контекст шифрования.

    HCRYPTKEY aes_key_; // Сессионный ключ для дешифрования.
    HCRYPTKEY rsa_key_;

    DWORD buffer_size_; // Размер буфера дешифрования в байтах.

    ScopedAlignedBuffer buffer_; // Буфер дешифрования.

    DISALLOW_COPY_AND_ASSIGN(DecryptorAES);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DECRYPTOR_AES_H
