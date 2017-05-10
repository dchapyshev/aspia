//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/sha512.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/sha512.h"
#include "base/logging.h"
#include "base/macros.h"

#include <wincrypt.h>

namespace aspia {

static const DWORD kExpectedHashSize = 64;

class ScopedCryptoProvider
{
public:
    ScopedCryptoProvider() : prov_(NULL) { }
    ~ScopedCryptoProvider()
    {
        if (prov_ != NULL)
            CryptReleaseContext(prov_, 0);
    }

    HCRYPTPROV* Recieve() { return &prov_; }
    operator HCRYPTPROV() { return prov_; }

private:
    HCRYPTPROV prov_;

    DISALLOW_COPY_AND_ASSIGN(ScopedCryptoProvider);
};

class ScopedCryptoHash
{
public:
    ScopedCryptoHash() : hash_(NULL) { }
    ~ScopedCryptoHash()
    {
        if (hash_ != NULL)
            CryptDestroyHash(hash_);
    }

    HCRYPTHASH* Recieve() { return &hash_; }
    operator HCRYPTHASH() { return hash_; }

private:
    HCRYPTHASH hash_;

    DISALLOW_COPY_AND_ASSIGN(ScopedCryptoHash);
};

bool CreateSHA512(const std::string& data, std::string& data_hash)
{
    ScopedCryptoProvider prov;

    if (!CryptAcquireContextW(prov.Recieve(),
                              NULL,
                              MS_ENH_RSA_AES_PROV_W,
                              PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
        LOG(ERROR) << "CryptAcquireContextW() failed: " << GetLastError();
        return false;
    }

    {
        ScopedCryptoHash hash;

        if (!CryptCreateHash(prov, CALG_SHA_512, NULL, 0, hash.Recieve()))
        {
            LOG(ERROR) << "CryptCreateHash() failed: " << GetLastError();
            return false;
        }

        if (!CryptHashData(hash,
                           reinterpret_cast<const BYTE*>(data.c_str()),
                           data.length(),
                           0))
        {
            LOG(ERROR) << "CryptHashData() failed: " << GetLastError();
            return false;
        }

        DWORD size = 0;

        if (!CryptGetHashParam(hash, HP_HASHVAL, NULL, &size, 0))
        {
            LOG(ERROR) << "CryptGetHashParam() failed: " << GetLastError();
            return false;
        }

        if (size != kExpectedHashSize)
        {
            LOG(ERROR) << "Wrong hash size: " << size;
            return false;
        }

        data_hash.resize(size);

        if (!CryptGetHashParam(hash,
                               HP_HASHVAL,
                               reinterpret_cast<BYTE*>(&data_hash[0]),
                               &size,
                               0))
        {
            LOG(ERROR) << "CryptGetHashParam() failed: " << GetLastError();
            return false;
        }
    }

    return true;
}

} // namespace aspia
