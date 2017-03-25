/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/sha512.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

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
    SHA512(const std::string &data, uint32_t iter);
    ~SHA512();

    const std::string& Hash() const;

    operator const std::string&() const;

private:
    std::string data_;

    DISALLOW_COPY_AND_ASSIGN(SHA512);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__SHA512_H
