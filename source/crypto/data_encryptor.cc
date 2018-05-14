//
// PROJECT:         Aspia
// FILE:            crypto/data_encryptor.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/data_encryptor.h"

#include <QCryptographicHash>

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

namespace aspia {

namespace {

const size_t kChunkSize = 4096;

} // namespace

// static
QByteArray DataEncryptor::createKey(const QByteArray& password,
                                    const QByteArray& salt,
                                    int rounds)
{
    QByteArray data = password;

    for (int i = 0; i < rounds; ++i)
    {
        QCryptographicHash key_hash(QCryptographicHash::Sha256);

        key_hash.addData(data);
        key_hash.addData(salt);

        data = key_hash.result();
    }

    return data;
}

// static
QByteArray DataEncryptor::encrypt(const QByteArray& source_data, const QByteArray& key)
{
    Q_ASSERT(key.size() == crypto_secretstream_xchacha20poly1305_KEYBYTES);

    QByteArray encrypted_data;
    encrypted_data.resize(crypto_secretstream_xchacha20poly1305_HEADERBYTES);

    crypto_secretstream_xchacha20poly1305_state state;

    crypto_secretstream_xchacha20poly1305_init_push(
        &state,
        reinterpret_cast<quint8*>(encrypted_data.data()),
        reinterpret_cast<const quint8*>(key.constData()));

    const quint8* input_buffer = reinterpret_cast<const quint8*>(source_data.constData());
    size_t input_pos = 0;

    bool end_of_buffer = false;

    do
    {
        size_t consumed = std::min(source_data.size() - input_pos, kChunkSize);
        quint8 tag = 0;

        if (consumed < kChunkSize)
        {
            tag = crypto_secretstream_xchacha20poly1305_TAG_FINAL;
            end_of_buffer = true;
        }

        quint8 output_buffer[kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES];
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
bool DataEncryptor::decrypt(const QByteArray& source_data,
                            const QByteArray& key,
                            QByteArray* decrypted_data)
{
    return decrypt(source_data.constData(), source_data.size(), key, decrypted_data);
}

// static
bool DataEncryptor::decrypt(const char* source_data, int source_size, const QByteArray& key,
                            QByteArray* decrypted_data)
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
            &state, reinterpret_cast<const quint8*>(source_data),
            reinterpret_cast<const quint8*>(key.constData())) != 0)
    {
        qWarning("crypto_secretstream_xchacha20poly1305_init_pull failed");
        return false;
    }

    const quint8* input_buffer = reinterpret_cast<const quint8*>(source_data) +
                                 crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    size_t input_size = source_size - crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    size_t input_pos = 0;

    bool end_of_buffer = false;

    do
    {
        size_t consumed = std::min(input_size - input_pos,
                                   kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES);

        quint8 output_buffer[kChunkSize];
        quint64 output_length;
        quint8 tag;

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
