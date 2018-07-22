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

#ifndef ASPIA_CRYPTO__ENCRYPTOR_H_
#define ASPIA_CRYPTO__ENCRYPTOR_H_

#include <QByteArray>

#include <vector>

namespace aspia {

// Implements encryption of messages with using xsalsa20 + poly1305 algorithms.
class Encryptor
{
public:
    enum class Mode { SERVER, CLIENT };

    explicit Encryptor(Mode mode);
    ~Encryptor();

    bool readHelloMessage(const QByteArray& message_buffer);
    QByteArray helloMessage();

    int encryptedDataSize(int source_data_size);
    bool encrypt(const char* source, int source_size, char* target);

    int decryptedDataSize(int source_data_size);
    bool decrypt(const char* source, int source_size, char* target);

private:
    const Mode mode_;

    std::vector<quint8> local_public_key_;
    std::vector<quint8> local_secret_key_;

    std::vector<quint8> encrypt_key_;
    std::vector<quint8> decrypt_key_;

    std::vector<quint8> encrypt_nonce_;
    std::vector<quint8> decrypt_nonce_;

    Q_DISABLE_COPY(Encryptor)
};

} // namespace aspia

#endif // ASPIA_CRYPTO__ENCRYPTOR_H_
