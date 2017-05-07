//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/io_buffer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__IO_BUFFER_H
#define _ASPIA_BASE__IO_BUFFER_H

#include "base/macros.h"

#include <memory>

namespace aspia {

class IOBuffer
{
public:
    explicit IOBuffer(size_t size) :
        data_size_(size)
    {
        data_.reset(new uint8_t[data_size_]);
    }

    ~IOBuffer() { }

    uint8_t* Data() const { return data_.get(); }
    size_t Size() const { return data_size_; }

private:
    std::unique_ptr<uint8_t[]> data_;
    const size_t data_size_;

    DISALLOW_COPY_AND_ASSIGN(IOBuffer);
};

} // namespace aspia

#endif // _ASPIA_BASE__IO_BUFFER_H
