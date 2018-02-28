//
// PROJECT:         Aspia
// FILE:            protocol/io_buffer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/io_buffer.h"

#include "base/logging.h"

namespace aspia {

IOBuffer::IOBuffer(size_t size)
    : data_size_(size)
{
    DCHECK_NE(data_size_, 0U);
    data_ = std::make_unique<uint8_t[]>(data_size_);
}

IOBuffer::IOBuffer(const IOBuffer& other)
{
    CopyFrom(other);
}

IOBuffer& IOBuffer::operator=(const IOBuffer& other)
{
    if (this != &other)
        CopyFrom(other);
    return *this;
}

IOBuffer::IOBuffer(IOBuffer&& other) noexcept
{
    data_ = std::move(other.data_);
    data_size_ = other.data_size_;
    other.data_size_ = 0;
}

IOBuffer& IOBuffer::operator=(IOBuffer&& other) noexcept
{
    data_ = std::move(other.data_);
    data_size_ = other.data_size_;
    other.data_size_ = 0;
    return *this;
}

// static
IOBuffer IOBuffer::Create(size_t size)
{
    return IOBuffer(size);
}

void IOBuffer::CopyFrom(const IOBuffer& other)
{
    data_ = std::make_unique<uint8_t[]>(other.data_size_);
    data_size_ = other.data_size_;

    memcpy(data_.get(), other.data_.get(), data_size_);
}

} // namespace aspia
