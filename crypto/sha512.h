//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/sha512.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SHA512_H
#define _ASPIA_CRYPTO__SHA512_H

#include <string>

namespace aspia {

bool CreateSHA512(const std::string& data, std::string& data_hash);

} // namespace aspia

#endif // _ASPIA_CRYPTO__SHA512_H
