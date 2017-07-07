//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/secure_io_buffer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SECURE_IO_BUFFER_H
#define _ASPIA_CRYPTO__SECURE_IO_BUFFER_H

#include "protocol/io_buffer.h"

extern "C" {
#define SODIUM_STATIC
#include <sodium.h>
} // extern "C"

namespace aspia {

class SecureIOBuffer : public IOBuffer
{
public:
    SecureIOBuffer() { }

    SecureIOBuffer(size_t size) :
        IOBuffer(size)
    {
        // Nothing
    }

    SecureIOBuffer(SecureIOBuffer&& other) :
        IOBuffer(std::move(other))
    {
        // Nothing
    }

    SecureIOBuffer(IOBuffer&& other) :
        IOBuffer(std::move(other))
    {
        // Nothing
    }

    ~SecureIOBuffer()
    {
        if (!IsEmpty())
        {
            sodium_memzero(data(), size());
        }
    }

private:
    DISALLOW_COPY_AND_ASSIGN(SecureIOBuffer);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__SECURE_IO_BUFFER_H
