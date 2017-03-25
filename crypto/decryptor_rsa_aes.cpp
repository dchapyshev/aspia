//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor_rsa_aes.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/decryptor_rsa_aes.h"

#include "base/logging.h"

namespace aspia {

// Размеры ключей шифрования в битах.
static const DWORD kRSAKeySize = 1024;

DecryptorAES::DecryptorAES() :
    prov_(NULL),
    rsa_key_(NULL),
    aes_key_(NULL)
{
    // Nothing
}

DecryptorAES::~DecryptorAES()
{
    Cleanup();
}

bool DecryptorAES::Init()
{
    Cleanup();

    // Создаем контекст шифрования.
    if (!CryptAcquireContextW(&prov_,
                              NULL,
                              MS_ENH_RSA_AES_PROV_W,
                              PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
        LOG(ERROR) << "CryptAcquireContextW() failed: " << GetLastError();
        return false;
    }

    // Мы поддерживаем RSA ключи только размера 2048 бит.
    static_assert(kRSAKeySize == 1024, "Not supported RSA key size");

    // Создаем экземпляр ключа для RSA шифрования.
    if (!CryptGenKey(prov_,
                     CALG_RSA_KEYX,
                     CRYPT_EXPORTABLE | (kRSAKeySize << 16),
                     &rsa_key_))
    {
        LOG(ERROR) << "CryptGenKey() failed: " << GetLastError();
        return false;
    }

    return true;
}

void DecryptorAES::Cleanup()
{
    //
    // Уничтожаем ключи шифрования. Ключи должны быть уничтожены до уничтожения
    // контекста шифрования.
    //

    if (aes_key_)
    {
        // Уничтожаем сессионный ключ.
        BOOL ret = CryptDestroyKey(aes_key_);
        DCHECK(ret) << GetLastError();
        aes_key_ = NULL;
    }

    if (rsa_key_)
    {
        // Уничтожаем ключ RSA.
        BOOL ret = CryptDestroyKey(rsa_key_);
        DCHECK(ret) << GetLastError();
        rsa_key_ = NULL;
    }

    if (prov_)
    {
        // Уничтожаем контекст шифрования.
        BOOL ret = CryptReleaseContext(prov_, 0);
        DCHECK(ret) << GetLastError();
        prov_ = NULL;
    }
}

uint32_t DecryptorAES::GetPublicKeySize()
{
    return (kRSAKeySize / 8);
}

bool DecryptorAES::GetPublicKey(uint8_t* key, uint32_t len)
{
    DWORD blob_size = 0;

    // Получаем размер, который необходим для хранения открытого ключа RSA.
    if (!CryptExportKey(rsa_key_, NULL, PUBLICKEYBLOB, 0, NULL, &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        return false;
    }

    // Выделяем память для открытого ключа RSA.
    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    // Экспортируем открытый ключ RSA в выделенный буфер.
    if (!CryptExportKey(rsa_key_, NULL, PUBLICKEYBLOB, 0, blob.get(), &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        return false;
    }

    if (sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY) + len != blob_size)
    {
        LOG(ERROR) << "Wrong size of public key: " << blob_size;
        return false;
    }

    memcpy(key, blob.get() + sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY), len);

    return true;
}

bool DecryptorAES::SetSessionKey(const uint8_t* key, uint32_t len)
{
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

    uint32_t blob_size = sizeof(header) + sizeof(alg_id) + len;
    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    memcpy(blob.get(), &header, sizeof(header));
    memcpy(blob.get() + sizeof(header), &alg_id, sizeof(alg_id));
    memcpy(blob.get() + sizeof(header) + sizeof(alg_id), key, len);

    if (!CryptImportKey(prov_, blob.get(), blob_size, rsa_key_, 0, &aes_key_))
    {
        LOG(ERROR) << "CryptImportKey() failed: " << GetLastError();
        return false;
    }

    DWORD mode = CRYPT_MODE_CBC;

    // Устанавливаем режим шифрования для ключа.
    if (!CryptSetKeyParam(aes_key_, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
    {
        LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();

        // Уничтожаем ключ шифрования.
        BOOL ret = CryptDestroyKey(aes_key_);
        DCHECK(ret) << GetLastError();
        aes_key_ = NULL;

        return false;
    }

    return true;
}

const uint8_t* DecryptorAES::Decrypt(const uint8_t* in, uint32_t in_len, uint32_t* out_len)
{
    //
    // Если размер уже инициализированного буфера меньше, чем размер
    // зашифрованного сообщения.
    //
    if (buffer_.Size() < in_len)
        buffer_.Resize(in_len);

    DWORD length = in_len;

    // Копируем зашифрованное сообщение в буфер для расшифровки.
    memcpy(buffer_.Get(), in, length);

    // Расшифровываем сообщение.
    if (!CryptDecrypt(aes_key_, NULL, TRUE, 0, buffer_.Get(), &length))
    {
        DLOG(ERROR) << "CryptDecrypt() failed: " << GetLastError();
        return nullptr;
    }

    // Инициализируем выходные параметры.
    *out_len = length;

    return buffer_.Get();
}

} // namespace aspia
