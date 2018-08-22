//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "crypto/data_encryptor.h"

#define SODIUM_STATIC
#include <sodium.h>

#include "crypto/sha.h"

namespace aspia {

namespace {

const size_t kChunkSize = 4096;

} // namespace

// static
std::string DataEncryptor::createKey(const std::string& password,
                                     const std::string& salt,
                                     int rounds)
{
    std::string data = password;

    for (int i = 0; i < rounds; ++i)
    {
        Sha256 hash;

        hash.addData(data);
        hash.addData(salt);

        data = hash.result();
    }

    return data;
}

// static
std::string DataEncryptor::encrypt(const std::string& source_data, const std::string& key)
{
    Q_ASSERT(key.size() == crypto_secretstream_xchacha20poly1305_KEYBYTES);

    std::string encrypted_data;
    encrypted_data.resize(crypto_secretstream_xchacha20poly1305_HEADERBYTES);

    crypto_secretstream_xchacha20poly1305_state state;

    crypto_secretstream_xchacha20poly1305_init_push(
        &state,
        reinterpret_cast<uint8_t*>(encrypted_data.data()),
        reinterpret_cast<const uint8_t*>(key.c_str()));

    const uint8_t* input_buffer = reinterpret_cast<const uint8_t*>(source_data.c_str());
    size_t input_pos = 0;

    bool end_of_buffer = false;

    do
    {
        size_t consumed = std::min(source_data.size() - input_pos, kChunkSize);
        uint8_t tag = 0;

        if (consumed < kChunkSize)
        {
            tag = crypto_secretstream_xchacha20poly1305_TAG_FINAL;
            end_of_buffer = true;
        }

        uint8_t output_buffer[kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES];
        quint64 output_length;

        crypto_secretstream_xchacha20poly1305_push(&state,
                                                   output_buffer, &output_length,
                                                   input_buffer + input_pos, consumed,
                                                   nullptr, 0,
                                                   tag);

        size_t old_size = encrypted_data.size();

        encrypted_data.resize(old_size + static_cast<size_t>(output_length));
        memcpy(encrypted_data.data() + old_size, output_buffer, static_cast<size_t>(output_length));

        input_pos += consumed;

    } while (!end_of_buffer);

    return encrypted_data;
}

// static
bool DataEncryptor::decrypt(const std::string& source_data,
                            const std::string& key,
                            std::string* decrypted_data)
{
    return decrypt(source_data.c_str(), source_data.size(), key, decrypted_data);
}

// static
bool DataEncryptor::decrypt(const char* source_data, int source_size, const std::string& key,
                            std::string* decrypted_data)
{
    if (!source_data || source_size < crypto_secretstream_xchacha20poly1305_HEADERBYTES ||
        !decrypted_data)
    {
        qWarning("Invalid parameters");
        return false;
    }

    if (key.size() != crypto_secretstream_xchacha20poly1305_KEYBYTES)
    {
        qWarning("Invalid key size");
        return false;
    }

    decrypted_data->clear();

    crypto_secretstream_xchacha20poly1305_state state;

    if (crypto_secretstream_xchacha20poly1305_init_pull(
            &state, reinterpret_cast<const uint8_t*>(source_data),
            reinterpret_cast<const uint8_t*>(key.c_str())) != 0)
    {
        qWarning("crypto_secretstream_xchacha20poly1305_init_pull failed");
        return false;
    }

    const uint8_t* input_buffer = reinterpret_cast<const uint8_t*>(source_data) +
                                 crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    size_t input_size = source_size - crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    size_t input_pos = 0;

    bool end_of_buffer = false;

    do
    {
        size_t consumed = std::min(input_size - input_pos,
                                   kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES);

        uint8_t output_buffer[kChunkSize];
        quint64 output_length;
        uint8_t tag;

        if (crypto_secretstream_xchacha20poly1305_pull(&state, output_buffer, &output_length, &tag,
                                                       input_buffer + input_pos, consumed, nullptr,
                                                       0) != 0)
        {
            qWarning("crypto_secretstream_xchacha20poly1305_pull failed");
            return false;
        }

        input_pos += consumed;

        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL)
        {
            if (input_pos != input_size)
            {
                qWarning("Unexpected end of buffer");
                return false;
            }

            end_of_buffer = true;
        }

        size_t old_size = decrypted_data->size();

        decrypted_data->resize(old_size + static_cast<size_t>(output_length));
        memcpy(decrypted_data->data() + old_size, output_buffer,
               static_cast<size_t>(output_length));

    } while (!end_of_buffer);

    return true;
}

} // namespace aspia
