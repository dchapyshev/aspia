//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/sha512.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SHA512_H
#define _ASPIA_CRYPTO__SHA512_H

#include "aspia_config.h"

#include <stdint.h>
#include <string>

#include "base/macros.h"

namespace aspia {

class SHA512
{
public:
    SHA512();
    ~SHA512();

    void Update(const uint8_t *buffer, size_t size);
    void Update(const std::string &buffer);

    typedef struct
    {
        uint8_t data[64];
        uint32_t size;
    } Hash;

    void Final(Hash *hash);

private:
    void Transform(const uint8_t *buffer);

private:
    uint64_t length_;
    uint64_t state_[8];
    size_t curlen_;
    uint8_t buf_[128];

    DISALLOW_COPY_AND_ASSIGN(SHA512);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__SHA512_H
