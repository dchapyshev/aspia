//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_aligned_buffer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_aligned_buffer.h"

#include <malloc.h>

#include "base/logging.h"

namespace aspia {

ScopedAlignedBuffer::ScopedAlignedBuffer() :
    buffer_(nullptr),
    size_(0)
{
    // Nothing
}

ScopedAlignedBuffer::~ScopedAlignedBuffer()
{
    _aligned_free(buffer_);
}

uint8_t* ScopedAlignedBuffer::Get() const
{
    return buffer_;
}

void ScopedAlignedBuffer::Resize(size_t size, size_t align)
{
    buffer_ = reinterpret_cast<uint8_t*>(_aligned_realloc(buffer_, size, align));
    CHECK(buffer_);
    size_ = size;
}

size_t ScopedAlignedBuffer::Size() const
{
    return size_;
}

ScopedAlignedBuffer::operator uint8_t*()
{
    return buffer_;
}

} // namespace aspia
