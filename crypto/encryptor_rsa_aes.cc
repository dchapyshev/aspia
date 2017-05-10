//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor_rsa_aes.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/encryptor_rsa_aes.h"
#include "base/logging.h"

namespace aspia {

// Размеры ключей шифрования в битах.
static const DWORD kAESKeySize = 256;
static const DWORD kRSAKeySize = 1024;

// static
std::unique_ptr<EncryptorRsaAes> EncryptorRsaAes::Create()
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

    static_assert(kAESKeySize == 256, "Not supported AES key size");

    HCRYPTKEY aes_key;

    if (!CryptGenKey(prov, CALG_AES_256, CRYPT_EXPORTABLE | (kAESKeySize << 16), &aes_key))
    {
        LOG(ERROR) << "CryptGenKey() failed: " << GetLastError();
        CryptReleaseContext(prov, 0);
        return nullptr;
    }

    DWORD mode = CRYPT_MODE_CBC;

    if (!CryptSetKeyParam(aes_key, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
    {
        LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();
        CryptDestroyKey(aes_key);
        CryptReleaseContext(prov, 0);
        return nullptr;
    }

    DWORD block_size;
    DWORD block_size_len = sizeof(block_size);

    if (!CryptGetKeyParam(aes_key,
                          KP_BLOCKLEN,
                          reinterpret_cast<BYTE*>(&block_size),
                          &block_size_len,
                          0))
    {
        LOG(ERROR) << "CryptGetKeyParam() failed: " << GetLastError();
        CryptDestroyKey(aes_key);
        CryptReleaseContext(prov, 0);
        return nullptr;
    }

    // Переводим биты в байты.
    block_size /= 8;

    return std::unique_ptr<EncryptorRsaAes>(
        new EncryptorRsaAes(prov, aes_key, block_size));
}

EncryptorRsaAes::EncryptorRsaAes(HCRYPTPROV prov, HCRYPTKEY aes_key, DWORD block_size) :
    prov_(prov),
    aes_key_(aes_key),
    block_size_(block_size)
{
    // Nothing
}

EncryptorRsaAes::~EncryptorRsaAes()
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

bool EncryptorRsaAes::SetPublicKey(const IOBuffer& public_key)
{
    if (public_key.IsEmpty())
        return false;

    // Согласно MSDN блоб публичного ключа имеет структуру:
    // PUBLICKEYSTRUC  publickeystruc;
    // RSAPUBKEY rsapubkey;
    // BYTE modulus[rsapubkey.bitlen / 8];

    PUBLICKEYSTRUC header = { 0 };

    header.bType    = PUBLICKEYBLOB;
    header.bVersion = CUR_BLOB_VERSION;
    header.reserved = 0;
    header.aiKeyAlg = CALG_RSA_KEYX;

    RSAPUBKEY rsa = { 0 };

    rsa.magic  = 0x31415352; // RSA1
    rsa.bitlen = kRSAKeySize;
    rsa.pubexp = 65537;

    uint32_t blob_size = sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY) + public_key.Size();
    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    memcpy(blob.get(), &header, sizeof(header));
    memcpy(blob.get() + sizeof(header), &rsa, sizeof(rsa));
    memcpy(blob.get() + sizeof(header) + sizeof(rsa), public_key.Data(), public_key.Size());

    if (!CryptImportKey(prov_, blob.get(), blob_size, NULL, 0, &rsa_key_))
    {
        LOG(ERROR) << "CryptImportKey() failed: " << GetLastError();
        return false;
    }

    return true;
}

IOBuffer EncryptorRsaAes::GetSessionKey()
{
    DWORD blob_size = 0;

    if (!CryptExportKey(aes_key_, rsa_key_, SIMPLEBLOB, 0, NULL, &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        return IOBuffer();
    }

    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    if (!CryptExportKey(aes_key_, rsa_key_, SIMPLEBLOB, 0, blob.get(), &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        return IOBuffer();
    }

    const size_t session_key_size = kRSAKeySize / 8;

    if (sizeof(PUBLICKEYSTRUC) + sizeof(ALG_ID) + session_key_size != blob_size)
    {
        LOG(ERROR) << "Wrong size of session key: " << blob_size;
        return IOBuffer();
    }

    IOBuffer session_key(session_key_size);

    memcpy(session_key.Data(),
           blob.get() + sizeof(PUBLICKEYSTRUC) + sizeof(ALG_ID),
           session_key_size);

    return session_key;
}

IOBuffer EncryptorRsaAes::Encrypt(const IOBuffer& source_buffer)
{
    if (source_buffer.IsEmpty())
        return IOBuffer();

    DWORD size = source_buffer.Size();

    DWORD enc_size = ((size / block_size_ + 1) * block_size_);

    IOBuffer output_buffer(enc_size);

    memcpy(output_buffer.Data(), source_buffer.Data(), size);

    if (!CryptEncrypt(aes_key_, NULL, TRUE, 0, output_buffer.Data(), &size, enc_size))
    {
        DLOG(ERROR) << "CryptEncrypt() failed: " << GetLastError();
        return IOBuffer();
    }

    return output_buffer;
}

} // namespace aspia
