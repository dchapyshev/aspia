//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor_rsa_aes.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/decryptor_rsa_aes.h"
#include "base/logging.h"

namespace aspia {

static const DWORD kRSAKeySize = 1024; // In bits.

// static
std::unique_ptr<DecryptorRsaAes> DecryptorRsaAes::Create()
{
    HCRYPTPROV prov;

    if (!CryptAcquireContextW(&prov,
                              NULL,
                              MS_ENH_RSA_AES_PROV_W,
                              PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
        LOG(ERROR) << "CryptAcquireContextW() failed: " << GetLastError();
        return nullptr;
    }

    static_assert(kRSAKeySize == 1024, "Not supported RSA key size");

    HCRYPTKEY rsa_key;

    if (!CryptGenKey(prov,
                     CALG_RSA_KEYX,
                     CRYPT_EXPORTABLE | (kRSAKeySize << 16),
                     &rsa_key))
    {
        LOG(ERROR) << "CryptGenKey() failed: " << GetLastError();
        CryptReleaseContext(prov, 0);
        return nullptr;
    }

    return std::unique_ptr<DecryptorRsaAes>(
        new DecryptorRsaAes(prov, rsa_key));
}

DecryptorRsaAes::DecryptorRsaAes(HCRYPTPROV prov, HCRYPTKEY rsa_key) :
    prov_(prov),
    rsa_key_(rsa_key)
{
    // Nothing
}

DecryptorRsaAes::~DecryptorRsaAes()
{
    if (aes_key_)
    {
        CryptDestroyKey(aes_key_);
        aes_key_ = NULL;
    }

    if (rsa_key_)
    {
        CryptDestroyKey(rsa_key_);
        rsa_key_ = NULL;
    }

    if (prov_)
    {
        CryptReleaseContext(prov_, 0);
        prov_ = NULL;
    }
}

IOBuffer DecryptorRsaAes::GetPublicKey()
{
    DWORD blob_size = 0;

    if (!CryptExportKey(rsa_key_, NULL, PUBLICKEYBLOB, 0, NULL, &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        return IOBuffer();
    }

    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    if (!CryptExportKey(rsa_key_, NULL, PUBLICKEYBLOB, 0, blob.get(), &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        return IOBuffer();
    }

    const size_t key_size = kRSAKeySize / 8;

    if (sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY) + key_size != blob_size)
    {
        LOG(ERROR) << "Wrong size of public key: " << blob_size;
        return IOBuffer();
    }

    IOBuffer key_buffer(key_size);

    memcpy(key_buffer.Data(),
           blob.get() + sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY),
           key_size);

    return key_buffer;
}

bool DecryptorRsaAes::SetSessionKey(const IOBuffer& session_key)
{
    if (session_key.IsEmpty())
        return false;

    // Согласно MSDN блоб сессионного ключа имеет структуру:
    // PUBLICKEYSTRUC  publickeystruc;
    // ALG_ID algid;
    // BYTE encryptedkey[rsapubkey.bitlen / 8];

    PUBLICKEYSTRUC header = { 0 };

    header.bType    = SIMPLEBLOB;
    header.bVersion = CUR_BLOB_VERSION;
    header.reserved = 0;
    header.aiKeyAlg = CALG_AES_256;

    ALG_ID alg_id = CALG_RSA_KEYX;

    uint32_t blob_size = sizeof(header) + sizeof(alg_id) + session_key.Size();
    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    memcpy(blob.get(), &header, sizeof(header));
    memcpy(blob.get() + sizeof(header), &alg_id, sizeof(alg_id));
    memcpy(blob.get() + sizeof(header) + sizeof(alg_id), session_key.Data(), session_key.Size());

    if (!CryptImportKey(prov_, blob.get(), blob_size, rsa_key_, 0, &aes_key_))
    {
        LOG(ERROR) << "CryptImportKey() failed: " << GetLastError();
        return false;
    }

    DWORD mode = CRYPT_MODE_CBC;

    if (!CryptSetKeyParam(aes_key_, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
    {
        LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();

        CryptDestroyKey(aes_key_);
        aes_key_ = NULL;

        return false;
    }

    return true;
}

IOBuffer DecryptorRsaAes::Decrypt(const IOBuffer& source_buffer)
{
    if (source_buffer.IsEmpty())
        return IOBuffer();

    DWORD length = source_buffer.Size();

    if (!CryptDecrypt(aes_key_, NULL, TRUE, 0, source_buffer.Data(), &length))
    {
        DLOG(ERROR) << "CryptDecrypt() failed: " << GetLastError();
        return IOBuffer();
    }

    IOBuffer output_buffer(length);

    memcpy(output_buffer.Data(), source_buffer.Data(), output_buffer.Size());

    return output_buffer;
}

} // namespace aspia
