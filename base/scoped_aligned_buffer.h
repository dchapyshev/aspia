//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_aligned_buffer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_ALIGNED_BUFFER_H
#define _ASPIA_BASE__SCOPED_ALIGNED_BUFFER_H

#include <stdint.h>
#include <cstring>

#include "base/macros.h"

namespace aspia {

class ScopedAlignedBuffer
{
public:
    ScopedAlignedBuffer();
    ~ScopedAlignedBuffer();

    uint8_t* Get() const;
    void Resize(size_t size, size_t align = 32);
    size_t Size() const;

    operator uint8_t*();

private:
    uint8_t* buffer_;
    size_t size_;

    DISALLOW_COPY_AND_ASSIGN(ScopedAlignedBuffer);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_ALIGNED_BUFFER_H
