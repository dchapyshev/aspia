/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/decryptor.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CRYPTO__DECRYPTOR_H
#define _ASPIA_CRYPTO__DECRYPTOR_H

#include <stdint.h>
#include <memory>

namespace aspia {

class Decryptor
{
public:
    Decryptor() {}
    virtual ~Decryptor() {}

    virtual uint32_t GetPublicKeySize() = 0;
    virtual void GetPublicKey(uint8_t *key, uint32_t len) = 0;
    virtual void SetSessionKey(const uint8_t *key, uint32_t len) = 0;

    virtual void Decrypt(const uint8_t *in, uint32_t in_len,
                         uint8_t **out, uint32_t *out_len) = 0;
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DECRYPTOR_H
