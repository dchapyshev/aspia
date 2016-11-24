/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/encryptor_aes.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "crypto/encryptor_aes.h"

// Размер ключа шифрования в битах.
static const DWORD kKeySize = 256;

// Размер вектора инициализации ключа в байтах.
static const DWORD kIvSize = 16;

EncryptorAES::EncryptorAES(const std::string &password) :
    prov_(NULL),
    enc_key_(NULL),
    dec_key_(NULL),
    enc_buffer_size_(0),
    dec_buffer_size_(0),
    has_enc_session_key_(false),
    has_dec_session_key_(false)
{
    // Создаем контекст шифрования.
    if (!CryptAcquireContextW(&prov_,
                              NULL,
                              MS_ENH_RSA_AES_PROV_W,
                              PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
        LOG(ERROR) << "CryptAcquireContextW() failed: " << GetLastError();
        Cleanup();
        return;
    }

    // Создаем экземпляр ключа для шифрования с использованием пароля.
    enc_key_ = CreateKeyFromPassword(prov_, password);
    if (!enc_key_)
    {
        Cleanup();
        return;
    }

    //
    // Мы не можем использовать один и тот же экземпляр ключа для шифрования и дешифрования
    // и создаем дубликат ключа.
    // CBC режим шифрования предполагает зависимость последующих зашифрованных блоков от
    // предыдущих и именно в ключе хранится информация о предыдущем состоянии.
    // Использование одного и того же экземпляра ключа приведет к повреждению данных и ошибке
    // NTE_BAD_DATA при дешифровке.
    //
    if (!CryptDuplicateKey(enc_key_, NULL, 0, &dec_key_))
    {
        LOG(ERROR) << "CryptDuplicateKey() failed: " << GetLastError();
        Cleanup();
        return;
    }

    DWORD block_size_len = sizeof(block_size_);

    // Получаем длинну блока шифрования в битах.
    if (!CryptGetKeyParam(enc_key_,
                          KP_BLOCKLEN,
                          reinterpret_cast<BYTE*>(&block_size_),
                          &block_size_len,
                          0))
    {
        LOG(ERROR) << "CryptGetKeyParam(KP_BLOCKLEN) failed: " << GetLastError();
        Cleanup();
        return;
    }

    // Преобразуем биты в байты.
    block_size_ /= 8;
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
    if (enc_key_)
    {
        // Уничтожаем ключ для шифрования.
        if (!CryptDestroyKey(enc_key_))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        enc_key_ = NULL;
    }

    if (dec_key_)
    {
        // Уничтожаем ключ для дешифрования.
        if (!CryptDestroyKey(dec_key_))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        dec_key_ = NULL;
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

// static
HCRYPTKEY EncryptorAES::CreateKeyFromPassword(HCRYPTPROV provider,
                                              const std::string &password)
{
    // В случае AES шифрования длинна ключа должна быть 128, 192 или 256 бит.
    static_assert(kKeySize == 128 || kKeySize == 192 || kKeySize == 256,
                  "Not supported key size");

    HCRYPTHASH hash;

    // Создаем SHA512 хеш.
    if (!CryptCreateHash(provider, CALG_SHA_512, 0, 0, &hash))
    {
        LOG(ERROR) << "CryptCreateHash() failed: " << GetLastError();
        return NULL;
    }

    // Хешируем пароль.
    if (!CryptHashData(hash,
                       reinterpret_cast<const BYTE*>(password.c_str()),
                       password.size(),
                       0))
    {
        LOG(ERROR) << "CryptHashData() failed: " << GetLastError();
        CryptDestroyHash(hash);
        return NULL;
    }

    ALG_ID algorithm = 0;
    DWORD flags = 0;

    switch (kKeySize)
    {
        case 128:
            algorithm = CALG_AES_128;
            flags |= (128 << 16);
            break;

        case 192:
            algorithm = CALG_AES_192;
            flags |= (192 << 16);
            break;

        case 256:
            algorithm = CALG_AES_256;
            flags |= (256 << 16);
            break;
    }

    HCRYPTKEY key = NULL;

    // Создаем экземпляр ключа с использованием SHA512 хеша.
    if (!CryptDeriveKey(provider, algorithm, hash, flags, &key))
    {
        LOG(ERROR) << "CryptDeriveKey() failed: " << GetLastError();
        CryptDestroyHash(hash);
        return NULL;
    }

    CryptDestroyHash(hash);

    DWORD mode = CRYPT_MODE_CBC;

    // Устанавливаем режим шифрования для ключа.
    if (!CryptSetKeyParam(key, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
    {
        LOG(ERROR) << "CryptSetKeyParam(KP_MODE) failed: " << GetLastError();

        // Уничтожаем ключ шифрования.
        if (!CryptDestroyKey(key))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        return NULL;
    }

    return key;
}

bool EncryptorAES::Encrypt(const uint8_t *in, uint32_t in_len,
                           uint8_t **out, uint32_t *out_len)
{
    //
    // Если кодировщик не имеет вектора инициализации ключа, то добавляем к
    // размеру закодированного сообщения размер вектора инициализации.
    //
    DWORD header_size = ((!has_enc_session_key_) ? kIvSize : 0);

    // Определяем размер, который необходим для зашифрованных данных.
    DWORD enc_size = ((in_len / block_size_ + 1) * block_size_) + header_size;

    // Если уже выделенный размер меньше, чем требуемый.
    if (enc_buffer_size_ < enc_size)
    {
        enc_buffer_size_ = enc_size;

        // Переинициализируем буфер для энкодера.
        enc_buffer_.reset(new uint8_t[enc_size]);

        // Обнуляем память.
        SecureZeroMemory(enc_buffer_.get(), enc_size);
    }

    //
    // При шифровании первого сообщения каждая из сторон генерирует сессионный ключ
    // (вектор инициализации ключа) и добавляет его в начало первого сообщения.
    // Данный ключ уникальный для каждой из сторон обмена.
    //

    // Если шифратор не имеет сессионного ключа.
    if (!has_enc_session_key_)
    {
        // Выделяем память для вектора инициализации.
        std::unique_ptr<uint8_t[]> iv(new uint8_t[kIvSize]);

        // Заполняем ее нулями до заполнения случайными значениями.
        SecureZeroMemory(iv.get(), kIvSize);

        // Заполняем вектор инициализации случайными значениями.
        if (!CryptGenRandom(prov_, kIvSize, iv.get()))
        {
            LOG(ERROR) << "CryptGenRandom() failed: " << GetLastError();
            return false;
        }

        // Копируем вектор инициализации в буфер закодированного сообщения.
        memcpy(enc_buffer_.get(), iv.get(), kIvSize);

        // Устанавливаем вектор инициализации для ключа.
        if (!CryptSetKeyParam(enc_key_, KP_IV, iv.get(), 0))
        {
            LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();
            return false;
        }

        // Снова заполняем ее нулями перед освобождением.
        SecureZeroMemory(iv.get(), kIvSize);

        has_enc_session_key_ = true;
    }

    // Копируем исходный буфер в буфер для шифрования.
    memcpy(enc_buffer_.get() + header_size, in, in_len);

    DWORD size = in_len;

    // Выполняем шифрование данных.
    if (!CryptEncrypt(enc_key_,
                      NULL,
                      TRUE,
                      0,
                      enc_buffer_.get() + header_size,
                      &size,
                      enc_size - header_size))
    {
        LOG(ERROR) << "CryptEncrypt() failed: " << GetLastError();
        return false;
    }

    // Инициализируем выходные параметры.
    *out = enc_buffer_.get();
    *out_len = size + header_size;

    return true;
}

bool EncryptorAES::Decrypt(const uint8_t *in, uint32_t in_len,
                           uint8_t **out, uint32_t *out_len)
{
    //
    // Если размер уже инициализированного буфера меньше, чем размер
    // зашифрованного сообщения.
    //
    if (dec_buffer_size_ < in_len)
    {
        dec_buffer_size_ = in_len;

        // Переинициализируем буфер для дешифрования.
        dec_buffer_.reset(new uint8_t[in_len]);

        // Обнуляем память буфера.
        SecureZeroMemory(dec_buffer_.get(), in_len);
    }

    DWORD header_size = 0;

    // Если дешифратор не имеет сессионного ключа.
    if (!has_dec_session_key_)
    {
        // Выделяем память для вектора инициализации.
        std::unique_ptr<uint8_t[]> iv(new uint8_t[kIvSize]);

        // Копируем вектор инициализации из сообщения.
        memcpy(iv.get(), in, kIvSize);

        // Устанавливаем вектор инициализации для ключа.
        if (!CryptSetKeyParam(dec_key_, KP_IV, iv.get(), 0))
        {
            LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();
            return false;
        }

        // Заполняем память нулями перед освобождением.
        SecureZeroMemory(iv.get(), kIvSize);

        header_size = kIvSize;

        has_dec_session_key_ = true;
    }

    DWORD length = in_len - header_size;

    // Копируем зашифрованное сообщение в буфер для расшифровки.
    memcpy(dec_buffer_.get(), in + header_size, length);

    // Расшифровываем сообщение.
    if (!CryptDecrypt(dec_key_, NULL, TRUE, 0, dec_buffer_.get(), &length))
    {
        LOG(ERROR) << "CryptDecrypt() failed: " << GetLastError();
        return false;
    }

    // Инициализируем выходные параметры.
    *out = dec_buffer_.get();
    *out_len = length;

    return true;
}
