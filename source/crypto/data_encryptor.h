//
// PROJECT:         Aspia
// FILE:            crypto/data_encryptor.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DATA_ENCRYPTOR_H
#define _ASPIA_CRYPTO__DATA_ENCRYPTOR_H

#include <QByteArray>

namespace aspia {

class DataEncryptor
{
public:
    // Creates a key from the password. |password| must be in UTF-8 encoding.
    static QByteArray createKey(const QByteArray& password,
                                const QByteArray& salt,
                                int rounds);

    static QByteArray encrypt(const QByteArray& source_data, const QByteArray& key);

    static bool decrypt(const QByteArray& source_data,
                        const QByteArray& key,
                        QByteArray* decrypted_data);

    static bool decrypt(const char* source_data,
                        int source_size,
                        const QByteArray& key,
                        QByteArray* decrypted_data);

private:
    Q_DISABLE_COPY(DataEncryptor)
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DATA_ENCRYPTOR_H
