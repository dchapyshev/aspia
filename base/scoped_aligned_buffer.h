/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_aligned_buffer.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_ALIGNED_BUFFER_H
#define _ASPIA_BASE__SCOPED_ALIGNED_BUFFER_H

#include <stdint.h>
#include <malloc.h>

class ScopedAlignedBuffer
{
public:
    ScopedAlignedBuffer(size_t size, size_t align)
    {
        buffer_ = reinterpret_cast<uint8_t*>(_aligned_malloc(size, align));
    }

    ~ScopedAlignedBuffer()
    {
        if (buffer_)
            _aligned_free(buffer_);
    }

    uint8_t* get() const
    {
        return buffer_;
    }

    operator uint8_t*() { return buffer_; }

private:
    uint8_t *buffer_;
};

#endif // _ASPIA_BASE__SCOPED_ALIGNED_BUFFER_H
