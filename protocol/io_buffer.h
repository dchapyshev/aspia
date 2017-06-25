//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/io_buffer.h
// LICENSE:         See top-level directory
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
        data_.reset(new uint8_t[data_size_]);
    }

    IOBuffer() = default;

    IOBuffer(IOBuffer&& other)
    {
        data_ = std::move(other.data_);
        data_size_ = other.data_size_;
        other.data_size_ = 0;
    }

    virtual ~IOBuffer() = default;

    uint8_t* data() const { return data_.get(); }
    size_t size() const { return data_size_; }
    bool IsEmpty() const { return data_ == nullptr || data_size_ == 0; }

    IOBuffer& operator=(IOBuffer&& other)
    {
        data_ = std::move(other.data_);
        data_size_ = other.data_size_;
        other.data_size_ = 0;
        return *this;
    }

private:
    std::unique_ptr<uint8_t[]> data_;
    size_t data_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(IOBuffer);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__IO_BUFFER_H
