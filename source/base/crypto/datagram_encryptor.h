//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CRYPTO_DATAGRAM_ENCRYPTOR_H
#define BASE_CRYPTO_DATAGRAM_ENCRYPTOR_H

#include <QByteArray>

#include "base/crypto/openssl_util.h"

namespace base {

// AEAD encryptor for datagrams with explicit counter-based nonce construction. Unlike
// StreamEncryptor, this class does not maintain internal IV state. The caller provides a counter
// for each packet, and the nonce is computed as base_iv XOR counter (WireGuard-style).
class DatagramEncryptor
{
public:
    ~DatagramEncryptor();

    static std::unique_ptr<DatagramEncryptor> createForAes256Gcm(
        const QByteArray& key, const QByteArray& iv);

    static std::unique_ptr<DatagramEncryptor> createForChaCha20Poly1305(
        const QByteArray& key, const QByteArray& iv);

    // Returns the size of encrypted output for a given input size.
    qint64 encryptedDataSize(qint64 in_size);

    // Encrypts data using the given counter to construct the nonce.
    // Output format: [16-byte tag][ciphertext].
    bool encrypt(quint64 counter, const void* in, qint64 in_size, void* out);
    bool encrypt(quint64 counter, const void* in, qint64 in_size,
                 const void* aad, qint64 aad_size, void* out);

private:
    DatagramEncryptor(EVP_CIPHER_CTX_ptr ctx, const QByteArray& iv);

    // Builds a 12-byte nonce by XOR-ing the counter into the last 8 bytes of base_iv.
    QByteArray buildNonce(quint64 counter) const;

    EVP_CIPHER_CTX_ptr ctx_;
    QByteArray base_iv_;

    Q_DISABLE_COPY_MOVE(DatagramEncryptor)
};

} // namespace base

#endif // BASE_CRYPTO_DATAGRAM_ENCRYPTOR_H
