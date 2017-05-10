//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor_rsa_aes.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DECRYPTOR_RSA_AES_H
#define _ASPIA_CRYPTO__DECRYPTOR_RSA_AES_H

#include <wincrypt.h>

#include "base/macros.h"
#include "crypto/decryptor.h"

namespace aspia {

class DecryptorAES : public Decryptor
{
public:
    ~DecryptorAES();

    static DecryptorAES* Create();

    IOBuffer GetPublicKey();
    bool SetSessionKey(const IOBuffer& session_key);

    IOBuffer Decrypt(const IOBuffer& source_buffer) override;

private:
    DecryptorAES(HCRYPTPROV prov, HCRYPTKEY rsa_key);

private:
    HCRYPTPROV prov_ = NULL;

    HCRYPTKEY aes_key_ = NULL;
    HCRYPTKEY rsa_key_ = NULL;

    DISALLOW_COPY_AND_ASSIGN(DecryptorAES);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DECRYPTOR_RSA_AES_H
