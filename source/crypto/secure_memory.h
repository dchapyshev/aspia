//
// PROJECT:         Aspia
// FILE:            crypto/secure_memory.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SECURE_MEMORY_H
#define _ASPIA_CRYPTO__SECURE_MEMORY_H

#include "base/macros.h"

#include <memory>
#include <string>

namespace aspia {

void SecureMemZero(void* buffer, size_t size);
void SecureMemZero(std::string& str);
void SecureMemZero(std::wstring& str);

template<class StringType>
class SecureString
{
public:
    SecureString() = default;

    SecureString(const typename StringType::value_type* buffer)
        : string_(buffer != nullptr ? buffer : StringType())
    {
        // Nothing
    }

    SecureString(SecureString&& other)
        : string_(std::move(other.string_))
    {
        // Nothing
    }

    SecureString& operator=(SecureString&& other)
    {
        reset();
        string_ = std::move(other.string_);
        return *this;
    }

    SecureString(StringType&& other_string)
        : string_(std::move(other_string))
    {
        // Nothing
    }

    SecureString& operator=(StringType&& other_string)
    {
        reset();
        string_ = std::move(other_string);
        return *this;
    }

    ~SecureString() { reset(); }

    const StringType& string() const { return string_; }
    StringType& mutable_string() { return string_; }

    void reset()
    {
        SecureMemZero(string_);
        string_.clear();
    }

    void reset(const StringType& string)
    {
        reset();
        string_ = string;
    }

private:
    typename StringType string_;

    DISALLOW_COPY_AND_ASSIGN(SecureString);
};

template<typename Type, size_t kArraySize>
class SecureArray
{
public:
    SecureArray() = default;
    ~SecureArray() { reset(); }

    Type* get(size_t index) { return &array_[index]; }
    Type* get() { return get(0); }
    size_t count() const { return kArraySize; }
    size_t size() const { return count() * sizeof(Type); }

    void reset()
    {
        SecureMemZero(get(), size());
    }

private:
    Type array_[kArraySize] = { 0 };

    DISALLOW_COPY_AND_ASSIGN(SecureArray);
};

class SecureBuffer
{
public:
    SecureBuffer() = default;
    explicit SecureBuffer(size_t size)
        : data_size_(size)
    {
        data_ = std::make_unique<uint8_t[]>(data_size_);
        SecureMemZero(data_.get(), data_size_);
    }

    ~SecureBuffer()
    {
        SecureMemZero(data_.get(), data_size_);
    }

    size_t size() const { return data_size_; }
    uint8_t* data() const { return data_.get(); }

private:
    std::unique_ptr<uint8_t[]> data_;
    size_t data_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(SecureBuffer);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__SECURE_MEMORY_H
