//
// PROJECT:         Aspia
// FILE:            crypto/encryptor.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

#include <QByteArray>

#include <vector>

namespace aspia {

// Implements encryption of messages with using xsalsa20 + poly1305 algorithms.
class Encryptor
{
public:
    enum Mode
    {
        ServerMode,
        ClientMode
    };

    explicit Encryptor(Mode mode);
    ~Encryptor();

    bool readHelloMessage(const QByteArray& message_buffer);
    QByteArray helloMessage();

    QByteArray encrypt(const QByteArray& source_buffer);
    QByteArray decrypt(const QByteArray& source_buffer);

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

#endif // _ASPIA_CRYPTO__ENCRYPTOR_H
