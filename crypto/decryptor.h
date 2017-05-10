//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DECRYPTOR_H
#define _ASPIA_CRYPTO__DECRYPTOR_H

#include "base/io_buffer.h"

namespace aspia {

class Decryptor
{
public:
    virtual ~Decryptor() = default;

    virtual IOBuffer Decrypt(const IOBuffer& source_buffer) = 0;
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DECRYPTOR_H
