//
// PROJECT:         Aspia
// FILE:            protocol/io_buffer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__IO_BUFFER_H
#define _ASPIA_PROTOCOL__IO_BUFFER_H

#include <memory>

namespace aspia {

class IOBuffer
{
public:
    IOBuffer() = default;

    IOBuffer(const IOBuffer& other);
    IOBuffer& operator=(const IOBuffer& other);

    IOBuffer(IOBuffer&& other) noexcept;
    IOBuffer& operator=(IOBuffer&& other) noexcept;

    virtual ~IOBuffer() = default;

    static IOBuffer Create(size_t size);

    uint8_t* Data() const { return data_.get(); }
    size_t Size() const { return data_size_; }
    bool IsEmpty() const { return data_size_ == 0 || data_ == nullptr; }
    void CopyFrom(const IOBuffer& other);

private:
    explicit IOBuffer(size_t size);

    std::unique_ptr<uint8_t[]> data_;
    size_t data_size_ = 0;
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__IO_BUFFER_H
