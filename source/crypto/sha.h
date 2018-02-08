//
// PROJECT:         Aspia
// FILE:            crypto/sha.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SHA_H
#define _ASPIA_CRYPTO__SHA_H

#include <string>

extern "C" {
#define SODIUM_STATIC

#pragma warning(push, 3)
#include <sodium.h>
#pragma warning(pop)
} // extern "C"

namespace aspia {

class StreamSHA512
{
public:
    StreamSHA512();
    ~StreamSHA512() = default;

    void AppendData(const std::string& data);
    std::string Final();

private:
    crypto_hash_sha512_state state_;
};

// Creates a hash of the content |data| and saves it to the |data_hash|.
// Parameter |iter_count| specifies how many iterations must be done.
std::string SHA512(const std::string& data, size_t iter_count);

std::string SHA256(const std::string& data, size_t iter_count);

} // namespace aspia

#endif // _ASPIA_CRYPTO__SHA_H
