//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DECRYPTOR_H
#define _ASPIA_CRYPTO__DECRYPTOR_H

#include <stdint.h>
#include <memory>

namespace aspia {

class Decryptor
{
public:
    Decryptor() {}
    virtual ~Decryptor() = default;

    virtual bool Init() = 0;
    virtual uint32_t GetPublicKeySize() = 0;
    virtual bool GetPublicKey(uint8_t* key, uint32_t len) = 0;
    virtual bool SetSessionKey(const uint8_t* key, uint32_t len) = 0;
    virtual const uint8_t* Decrypt(const uint8_t* in, uint32_t in_len, uint32_t* out_len) = 0;
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DECRYPTOR_H
