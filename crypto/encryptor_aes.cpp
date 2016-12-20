/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/encryptor_aes.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "crypto/encryptor_aes.h"

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

// Размеры ключей шифрования в битах.
static const DWORD kAESKeySize = 256;
static const DWORD kRSAKeySize = 2048;

EncryptorAES::EncryptorAES() :
    prov_(NULL),
    rsa_key_(NULL),
    aes_key_(NULL),
    buffer_size_(0)
{
    while (true)
    {
        // Создаем контекст шифрования.
        if (!CryptAcquireContextW(&prov_,
                              NULL,
                              MS_ENH_RSA_AES_PROV_W,
                              PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
        {
            LOG(ERROR) << "CryptAcquireContextW() failed: " << GetLastError();
            break;
        }

        // В случае AES шифрования длинна ключа должна быть 256 бит.
        static_assert(kAESKeySize == 256, "Not supported AES key size");

        // Создаем экземпляр ключа для AES шифрования.
        if (!CryptGenKey(prov_, CALG_AES_256, CRYPT_EXPORTABLE | (kAESKeySize << 16), &aes_key_))
        {
            LOG(ERROR) << "CryptGenKey() failed: " << GetLastError();
            break;
        }

        DWORD mode = CRYPT_MODE_CBC;

        // Устанавливаем режим шифрования для ключа.
        if (!CryptSetKeyParam(aes_key_, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
        {
            LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();
            break;
        }

        DWORD block_size_len = sizeof(block_size_);

        // Получаем длинну блока шифрования в битах.
        if (!CryptGetKeyParam(aes_key_,
                              KP_BLOCKLEN,
                              reinterpret_cast<BYTE*>(&block_size_),
                              &block_size_len,
                              0))
        {
            LOG(ERROR) << "CryptGetKeyParam() failed: " << GetLastError();
            break;
        }

        // Переводим биты в байты.
        block_size_ /= 8;

        return;
    }

    Cleanup();
    throw Exception("Unable to initialize encryptor.");
}

EncryptorAES::~EncryptorAES()
{
    Cleanup();
}

void EncryptorAES::Cleanup()
{
    //
    // Уничтожаем ключи шифрования. Ключи должны быть уничтожены до уничтожения
    // контекста шифрования.
    //
    if (aes_key_)
    {
        // Уничтожаем ключ для шифрования.
        if (!CryptDestroyKey(aes_key_))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        aes_key_ = NULL;
    }

    if (rsa_key_)
    {
        // Уничтожаем ключ RSA.
        if (!CryptDestroyKey(rsa_key_))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        rsa_key_ = NULL;
    }

    if (prov_)
    {
        // Уничтожаем контекст шифрования.
        if (!CryptReleaseContext(prov_, 0))
        {
            DLOG(ERROR) << "CryptReleaseContext() failed: " << GetLastError();
        }

        prov_ = NULL;
    }
}

void EncryptorAES::SetPublicKey(const uint8_t *key, uint32_t len)
{
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

    uint32_t blob_size = sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY) + len;
    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    memcpy(blob.get(), &header, sizeof(header));
    memcpy(blob.get() + sizeof(header), &rsa, sizeof(rsa));
    memcpy(blob.get() + sizeof(header) + sizeof(rsa), key, len);

    if (!CryptImportKey(prov_, blob.get(), blob_size, NULL, 0, &rsa_key_))
    {
        LOG(ERROR) << "CryptImportKey() failed: " << GetLastError();
        throw Exception("Unable to import public key.");
    }
}

uint32_t EncryptorAES::GetSessionKeySize()
{
    return (kRSAKeySize / 8);
}

void EncryptorAES::GetSessionKey(uint8_t *key, uint32_t len)
{
    DWORD blob_size = 0;

    // Получаем размер, который необходим для хранения зашифрованного ключа AES.
    if (!CryptExportKey(aes_key_, rsa_key_, SIMPLEBLOB, 0, NULL, &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        throw Exception("Unable to export session key.");
    }

    // Выделяем память для ключа AES.
    std::unique_ptr<uint8_t[]> blob(new uint8_t[blob_size]);

    // Заполняем выделенную память нулями.
    memset(blob.get(), 0, blob_size);

    // Экспортируем зашифрованный ключ AES в выделенный буфер.
    if (!CryptExportKey(aes_key_, rsa_key_, SIMPLEBLOB, 0, blob.get(), &blob_size))
    {
        LOG(ERROR) << "CryptExportKey() failed: " << GetLastError();
        throw Exception("Unable to export session key.");
    }

    if (sizeof(PUBLICKEYSTRUC) + sizeof(ALG_ID) + len != blob_size)
    {
        LOG(ERROR) << "Wrong size of session key: " << blob_size;
        throw Exception("Unable to export session key.");
    }

    memcpy(key, blob.get() + sizeof(PUBLICKEYSTRUC) + sizeof(ALG_ID), len);
}

void EncryptorAES::Encrypt(const uint8_t *in, uint32_t in_len,
                           uint8_t **out, uint32_t *out_len)
{
    // Определяем размер, который необходим для зашифрованных данных.
    DWORD enc_size = ((in_len / block_size_ + 1) * block_size_);

    // Если уже выделенный размер меньше, чем требуемый.
    if (buffer_size_ < enc_size)
    {
        buffer_size_ = enc_size;

        // Переинициализируем буфер для энкодера.
        buffer_.reset(new ScopedAlignedBuffer(enc_size));
    }

    // Копируем исходный буфер в буфер для шифрования.
    memcpy(buffer_->get(), in, in_len);

    DWORD size = in_len;

    // Выполняем шифрование данных.
    if (!CryptEncrypt(aes_key_, NULL, TRUE, 0, buffer_->get(), &size, enc_size))
    {
        LOG(ERROR) << "CryptEncrypt() failed: " << GetLastError();
        throw Exception("Unable to encrypt the message.");
    }

    // Инициализируем выходные параметры.
    *out = buffer_->get();
    *out_len = size;
}

} // namespace aspia
