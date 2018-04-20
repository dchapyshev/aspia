//
// PROJECT:         Aspia
// FILE:            crypto/string_encryptor.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/string_encryptor.h"

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

#include <algorithm>

namespace aspia {

namespace {

const size_t kChunkSize = 4096;

} // namespace

std::string EncryptString(const std::string& string, const QByteArray& key)
{
    Q_ASSERT(key.size() == crypto_secretstream_xchacha20poly1305_KEYBYTES);

    std::string encrypted_string;
    encrypted_string.resize(crypto_secretstream_xchacha20poly1305_HEADERBYTES);

    crypto_secretstream_xchacha20poly1305_state state;

    crypto_secretstream_xchacha20poly1305_init_push(
        &state,
        reinterpret_cast<uint8_t*>(encrypted_string.data()),
        reinterpret_cast<const uint8_t*>(key.data()));

    const uint8_t* input_buffer = reinterpret_cast<const uint8_t*>(string.c_str());
    size_t input_pos = 0;

    bool end_of_buffer = false;

    do
    {
        size_t consumed = std::min(string.size() - input_pos, kChunkSize);
        uint8_t tag = 0;

        if (consumed < kChunkSize)
        {
            tag = crypto_secretstream_xchacha20poly1305_TAG_FINAL;
            end_of_buffer = true;
        }

        uint8_t output_buffer[kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES];
        uint64_t output_length;

        crypto_secretstream_xchacha20poly1305_push(&state,
                                                   output_buffer, &output_length,
                                                   input_buffer + input_pos, consumed,
                                                   nullptr, 0,
                                                   tag);

        size_t old_size = encrypted_string.size();

        encrypted_string.resize(old_size + static_cast<size_t>(output_length));
        memcpy(&encrypted_string[old_size], output_buffer, static_cast<size_t>(output_length));

        input_pos += consumed;

    } while (!end_of_buffer);

    return encrypted_string;
}

bool DecryptString(const std::string& string, const QByteArray& key, std::string& decrypted_string)
{
    Q_ASSERT(key.size() == crypto_secretstream_xchacha20poly1305_KEYBYTES);

    decrypted_string.clear();

    if (string.size() < crypto_secretstream_xchacha20poly1305_HEADERBYTES)
        return false;

    crypto_secretstream_xchacha20poly1305_state state;

    if (crypto_secretstream_xchacha20poly1305_init_pull(
            &state,
            reinterpret_cast<const uint8_t*>(string.c_str()),
            reinterpret_cast<const uint8_t*>(key.data())) != 0)
    {
        qWarning("crypto_secretstream_xchacha20poly1305_init_pull failed");
        return false;
    }

    const uint8_t* input_buffer = reinterpret_cast<const uint8_t*>(string.c_str()) +
                                  crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    size_t input_size = string.size() - crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    size_t input_pos = 0;

    bool end_of_buffer = false;

    do
    {
        size_t consumed = std::min(input_size - input_pos,
                                   kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES);

        uint8_t output_buffer[kChunkSize];
        uint64_t output_length;
        uint8_t tag;

        if (crypto_secretstream_xchacha20poly1305_pull(&state,
                                                       output_buffer, &output_length,
                                                       &tag,
                                                       input_buffer + input_pos, consumed,
                                                       nullptr, 0) != 0)
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

        size_t old_size = decrypted_string.size();

        decrypted_string.resize(old_size + static_cast<size_t>(output_length));
        memcpy(&decrypted_string[old_size], output_buffer, static_cast<size_t>(output_length));

    } while (!end_of_buffer);

    return true;
}

} // namespace aspia
