/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/encryptor.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

#include <stdint.h>
#include <memory>

namespace aspia {

class Encryptor
{
public:
    Encryptor() {}
    virtual ~Encryptor() {}

    virtual uint32_t GetSessionKeySize() = 0;
    virtual void GetSessionKey(uint8_t *key, uint32_t len) = 0;
    virtual void SetPublicKey(const uint8_t *key, uint32_t len) = 0;

    virtual void Encrypt(const uint8_t *in, uint32_t in_len,
                         uint8_t **out, uint32_t *out_len) = 0;
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__ENCRYPTOR_H
