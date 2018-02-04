//
// PROJECT:         Aspia
// FILE:            crypto/string_encryptor.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__STRING_ENCRYPTOR_H
#define _ASPIA_CRYPTO__STRING_ENCRYPTOR_H

#include <string>

namespace aspia {

std::string EncryptString(const std::string& string, const std::string& key);

bool DecryptString(
    const std::string& string, const std::string& key, std::string& decrypted_string);

} // namespace aspia

#endif // _ASPIA_CRYPTO__STRING_ENCRYPTOR_H
