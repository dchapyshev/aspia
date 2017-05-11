//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/sha512.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SHA512_H
#define _ASPIA_CRYPTO__SHA512_H

#include "base/macros.h"

#include <string>

namespace aspia {

class SHA512
{
public:
    static const size_t kHashSize = 64;

    SHA512();
    ~SHA512() = default;

    bool Update(const std::string& data);
    bool Final(std::string& data_hash);

private:
    void Transform(const uint8_t* buffer);

    uint64_t length_ = 0;
    uint64_t state_[8];
    size_t curlen_ = 0;
    uint8_t buf_[128];

    DISALLOW_COPY_AND_ASSIGN(SHA512);
};

bool CreateSHA512(const std::string& data, std::string& data_hash);

} // namespace aspia

#endif // _ASPIA_CRYPTO__SHA512_H
