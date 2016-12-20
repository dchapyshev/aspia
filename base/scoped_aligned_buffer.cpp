/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_aligned_buffer.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/scoped_aligned_buffer.h"

#include <malloc.h>

#include "base/logging.h"

namespace aspia {

ScopedAlignedBuffer::ScopedAlignedBuffer(size_t size, size_t align) :
    buffer_(reinterpret_cast<uint8_t*>(_aligned_malloc(size, align)))
{
    CHECK(buffer_);
    memset(buffer_, 0, size);
}

ScopedAlignedBuffer::~ScopedAlignedBuffer()
{
    _aligned_free(buffer_);
}

uint8_t* ScopedAlignedBuffer::get() const
{
    return buffer_;
}

ScopedAlignedBuffer::operator uint8_t*()
{
    return buffer_;
}

} // namespace aspia
