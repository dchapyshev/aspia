/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/encryptor_aes.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_AES_H
#define _ASPIA_CRYPTO__ENCRYPTOR_AES_H

#include "aspia_config.h"

#include <wincrypt.h>
#include <memory>

#include "base/macros.h"
#include "base/logging.h"

#include "crypto/encryptor.h"

class EncryptorAES : public Encryptor
{
public:
    EncryptorAES(const std::string &password);
    ~EncryptorAES();

    bool Encrypt(const uint8_t *in, uint32_t in_len,
                 uint8_t **out, uint32_t *out_len) override;

    bool Decrypt(const uint8_t *in, uint32_t in_len,
                 uint8_t **out, uint32_t *out_len) override;

private:
    void Cleanup();

    static HCRYPTKEY CreateKeyFromPassword(HCRYPTPROV provider,
                                           const std::string &password);

private:
    HCRYPTPROV prov_; // Контекст шифрования.

    HCRYPTKEY enc_key_; // Ключ для шифрования.
    HCRYPTKEY dec_key_; // Ключ для дешифрования.

    bool has_enc_session_key_;
    bool has_dec_session_key_;

    DWORD block_size_; // Размер блока шифрования в байтах.

    DWORD enc_buffer_size_; // Размер буфера шифрования в байтах.
    DWORD dec_buffer_size_; // Размер буфера дешифрования в байтах.

    std::unique_ptr<uint8_t[]> enc_buffer_; // Буфер шифрования.
    std::unique_ptr<uint8_t[]> dec_buffer_; // Буфер дешифрования.

    DISALLOW_COPY_AND_ASSIGN(EncryptorAES);
};

#endif // _ASPIA_CRYPTO__ENCRYPTOR_AES_H
