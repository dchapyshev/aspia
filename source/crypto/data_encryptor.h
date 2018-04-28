//
// PROJECT:         Aspia
// FILE:            crypto/data_encryptor.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DATA_ENCRYPTOR_H
#define _ASPIA_CRYPTO__DATA_ENCRYPTOR_H

#include <string>

namespace aspia {

class DataEncryptor
{
public:
    // Creates a key from the password. |password| must be in UTF-8 encoding.
    static std::string createKey(const std::string& password);

    static std::string encrypt(const std::string& source_data, const std::string& key);

    static bool decrypt(const std::string& source_data,
                        const std::string& key,
                        std::string* decrypted_data);

private:
    Q_DISABLE_COPY(DataEncryptor)
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DATA_ENCRYPTOR_H
