//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/io_buffer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__IO_BUFFER_H
#define _ASPIA_PROTOCOL__IO_BUFFER_H

#include "base/macros.h"
#include "base/logging.h"

#include <memory>

namespace aspia {

class IOBuffer
{
public:
    explicit IOBuffer(size_t size) :
        data_size_(size)
    {
        DCHECK_NE(data_size_, 0);
        data_ = std::make_unique<uint8_t[]>(data_size_);
    }

    virtual ~IOBuffer() = default;

    uint8_t* data() const { return data_.get(); }
    size_t size() const { return data_size_; }

private:
    std::unique_ptr<uint8_t[]> data_;
    const size_t data_size_;

    DISALLOW_COPY_AND_ASSIGN(IOBuffer);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__IO_BUFFER_H
