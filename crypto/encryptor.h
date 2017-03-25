//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

#include <stdint.h>
#include <memory>

namespace aspia {

class Encryptor
{
public:
    Encryptor() {}
    virtual ~Encryptor() = default;

    virtual bool Init() = 0;
    virtual uint32_t GetSessionKeySize() = 0;
    virtual bool GetSessionKey(uint8_t* key, uint32_t len) = 0;
    virtual bool SetPublicKey(const uint8_t* key, uint32_t len) = 0;
    virtual const uint8_t* Encrypt(const uint8_t* in, uint32_t in_len, uint32_t* out_len) = 0;
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__ENCRYPTOR_H
