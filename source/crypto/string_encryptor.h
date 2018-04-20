//
// PROJECT:         Aspia
// FILE:            crypto/string_encryptor.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__STRING_ENCRYPTOR_H
#define _ASPIA_CRYPTO__STRING_ENCRYPTOR_H

#include <QByteArray>
#include <string>

namespace aspia {

std::string EncryptString(const std::string& string, const QByteArray& key);

bool DecryptString(
    const std::string& string, const QByteArray& key, std::string& decrypted_string);

} // namespace aspia

#endif // _ASPIA_CRYPTO__STRING_ENCRYPTOR_H
