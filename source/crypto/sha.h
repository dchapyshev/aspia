//
// PROJECT:         Aspia
// FILE:            crypto/sha.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SHA_H
#define _ASPIA_CRYPTO__SHA_H

#include <string>

namespace aspia {

// Creates a hash of the content |data| and saves it to the |data_hash|.
// Parameter |iter_count| specifies how many iterations must be done.
bool SHA512(const std::string& data, std::string& data_hash, size_t iter_count);

bool SHA256(const std::string& data, std::string& data_hash, size_t iter_count);

} // namespace aspia

#endif // _ASPIA_CRYPTO__SHA_H
