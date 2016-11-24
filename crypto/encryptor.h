/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/encryptor.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

class Encryptor
{
public:
    Encryptor() {}
    virtual ~Encryptor() {}

    virtual bool Encrypt(const uint8_t *in, uint32_t in_len,
                         uint8_t **out, uint32_t *out_len) = 0;

    virtual bool Decrypt(const uint8_t *in, uint32_t in_len,
                         uint8_t **out, uint32_t *out_len) = 0;
};

#endif // _ASPIA_CRYPTO__ENCRYPTOR_H
