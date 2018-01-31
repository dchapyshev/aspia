//
// PROJECT:         Aspia
// FILE:            crypto/string_encryptor.cc
// LICENSE:         Mozilla Public License Version 2.0
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

#include "base/logging.h"
#include "crypto/secure_memory.h"

namespace aspia {

namespace {

const size_t kChunkSize = 4096;

} // namespace

std::string EncryptString(const std::string& string, const std::string& key)
{
    DCHECK_EQ(key.size(), crypto_secretstream_xchacha20poly1305_KEYBYTES);

    if (string.empty())
        return std::string();

    std::string encrypted_string;
    encrypted_string.resize(crypto_secretstream_xchacha20poly1305_HEADERBYTES);

    crypto_secretstream_xchacha20poly1305_state state;

    crypto_secretstream_xchacha20poly1305_init_push(
        &state,
        reinterpret_cast<uint8_t*>(encrypted_string.data()),
        reinterpret_cast<const uint8_t*>(key.c_str()));

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

        encrypted_string.resize(encrypted_string.size() + static_cast<size_t>(output_length));

        memcpy(encrypted_string.data() - output_length,
               output_buffer,
               static_cast<size_t>(output_length));

        SecureMemZero(output_buffer, sizeof(output_buffer));

        input_pos += consumed;

    } while (!end_of_buffer);

    return encrypted_string;
}

std::string DecryptString(const std::string& string, const std::string& key)
{
    DCHECK_EQ(key.size(), crypto_secretstream_xchacha20poly1305_KEYBYTES);

    if (string.size() < crypto_secretstream_xchacha20poly1305_HEADERBYTES)
        return std::string();

    crypto_secretstream_xchacha20poly1305_state state;

    if (crypto_secretstream_xchacha20poly1305_init_pull(
            &state,
            reinterpret_cast<const uint8_t*>(string.c_str()),
            reinterpret_cast<const uint8_t*>(key.c_str())) != 0)
    {
        LOG(LS_WARNING) << "crypto_secretstream_xchacha20poly1305_init_pull failed";
        return std::string();
    }

    const uint8_t* input_buffer = reinterpret_cast<const uint8_t*>(string.c_str()) +
                                  crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    size_t input_pos = 0;

    std::string decrypted_string;
    bool end_of_buffer = false;

    do
    {
        size_t consumed = std::min(string.size() - input_pos, kChunkSize);

        uint8_t output_buffer[kChunkSize];
        uint64_t output_length;
        uint8_t tag;

        if (crypto_secretstream_xchacha20poly1305_pull(&state,
                                                       output_buffer, &output_length,
                                                       &tag,
                                                       input_buffer + input_pos, consumed,
                                                       nullptr, 0) != 0)
        {
            LOG(LS_WARNING) << "crypto_secretstream_xchacha20poly1305_pull failed";

            SecureMemZero(output_buffer, sizeof(output_buffer));
            SecureMemZero(decrypted_string);

            return std::string();
        }

        input_pos += consumed;

        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL)
        {
            if (input_pos != string.size())
            {
                LOG(LS_ERROR) << "Unexpected end of buffer";

                SecureMemZero(output_buffer, sizeof(output_buffer));
                SecureMemZero(decrypted_string);

                return std::string();
            }

            end_of_buffer = true;
        }

        decrypted_string.resize(decrypted_string.size() + static_cast<size_t>(output_length));

        memcpy(decrypted_string.data() - output_length,
               output_buffer,
               static_cast<size_t>(output_length));

        SecureMemZero(output_buffer, sizeof(output_buffer));

    } while (!end_of_buffer);

    return decrypted_string;
}

} // namespace aspia
