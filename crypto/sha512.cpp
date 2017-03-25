/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/sha512.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "crypto/sha512.h"

#include <wincrypt.h>

#include "base/logging.h"

namespace aspia {

static const DWORD kExpectedHashSize = 64;

SHA512::SHA512(const std::string &data, uint32_t iter) :
    data_(data)
{
    HCRYPTPROV prov = NULL;

    // Создаем контекст шифрования.
    if (!CryptAcquireContextW(&prov,
                              NULL,
                              MS_ENH_RSA_AES_PROV_W,
                              PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
        LOG(ERROR) << "CryptAcquireContextW() failed: " << GetLastError();
        throw Exception("Unable to initialize SHA512 context.");
    }

    // Выполняем вычисление хеша указанное количество итераций.
    for (uint32_t index = 0; index < iter; ++index)
    {
        HCRYPTHASH hash = NULL;

        // Создаем хеш SHA512.
        if (!CryptCreateHash(prov, CALG_SHA_512, NULL, 0, &hash))
        {
            LOG(ERROR) << "CryptCreateHash() failed: " << GetLastError();
            throw Exception("Unable to create SHA512 hash.");
        }

        // Хешируем данные в буфере.
        if (!CryptHashData(hash,
                           reinterpret_cast<const BYTE*>(data_.c_str()),
                           data_.length(),
                           0))
        {
            LOG(ERROR) << "CryptHashData() failed: " << GetLastError();
            throw Exception("Unable to create SHA512 hash.");
        }

        DWORD size = 0;

        // Получаем размер хеша в байтах.
        if (!CryptGetHashParam(hash, HP_HASHVAL, NULL, &size, 0))
        {
            LOG(ERROR) << "CryptGetHashParam() failed: " << GetLastError();
            throw Exception("Unable to create SHA512 hash.");
        }

        // Если размер хеша не совпадает с ожидаемым (64 байта для SHA512).
        if (size != kExpectedHashSize)
        {
            LOG(ERROR) << "Wrong hash size: " << size;
            throw Exception("Wrong SHA512 hash size.");
        }

        // Изменяем размер буфера по размеру хеша.
        data_.resize(size);

        // Экспортируем хеш в буфер.
        if (!CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE*>(&data_[0]), &size, 0))
        {
            LOG(ERROR) << "CryptGetHashParam() failed: " << GetLastError();
            throw Exception("Unable to create SHA512 hash.");
        }

        // Уничтожаем хеш.
        if (!CryptDestroyHash(hash))
        {
            DLOG(ERROR) << "CryptDestroyHash() failed: " << GetLastError();
        }
    }

    // Уничтожаем контекст шифрования.
    if (!CryptReleaseContext(prov, 0))
    {
        DLOG(ERROR) << "CryptReleaseContext() failed: " << GetLastError();
    }
}

SHA512::~SHA512()
{
    SecureZeroMemory(&data_[0], data_.size());
}

const std::string& SHA512::Hash() const
{
    return data_;
}

SHA512::operator const std::string&() const
{
    return Hash();
}

} // namespace aspia
