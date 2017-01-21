//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor_aes.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_AES_H
#define _ASPIA_CRYPTO__ENCRYPTOR_AES_H

#include "aspia_config.h"

#include <wincrypt.h>

#include "base/macros.h"
#include "base/scoped_aligned_buffer.h"

#include "crypto/encryptor.h"

namespace aspia {

class EncryptorAES : public Encryptor
{
public:
    EncryptorAES();
    ~EncryptorAES();

    uint32_t GetSessionKeySize() override;
    void GetSessionKey(uint8_t *key, uint32_t len) override;
    void SetPublicKey(const uint8_t *key, uint32_t len) override;

    void Encrypt(const uint8_t *in, uint32_t in_len,
                 uint8_t **out, uint32_t *out_len) override;

private:
    void Cleanup();

private:
    HCRYPTPROV prov_; // Контекст шифрования.

    HCRYPTKEY aes_key_; // Сессионный ключ для шифрования.
    HCRYPTKEY rsa_key_;

    DWORD block_size_; // Размер блока шифрования в байтах.

    DWORD buffer_size_; // Размер буфера шифрования в байтах.

    ScopedAlignedBuffer buffer_; // Буфер шифрования.

    DISALLOW_COPY_AND_ASSIGN(EncryptorAES);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__ENCRYPTOR_AES_H
