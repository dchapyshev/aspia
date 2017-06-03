//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/secure_buffer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SECURE_BUFFER_H
#define _ASPIA_CRYPTO__SECURE_BUFFER_H

#include "base/macros.h"

extern "C" {
#define SODIUM_STATIC
#include <sodium.h>
} // extern "C"

#include <memory>

namespace aspia {

class SecureBuffer
{
public:
    SecureBuffer() = default;

    explicit SecureBuffer(size_t size) :
        data_size_(size)
    {
        data_.reset(new uint8_t[data_size_]);
        sodium_memzero(data_.get(), data_size_);
    }

    ~SecureBuffer()
    {
        sodium_memzero(data_.get(), data_size_);
    }

    size_t size() const { return data_size_; }
    uint8_t* data() const { return data_.get(); }

private:
    std::unique_ptr<uint8_t[]> data_;
    size_t data_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(SecureBuffer);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__SECURE_BUFFER_H
