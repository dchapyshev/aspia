//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef BASE_MEMORY_TYPED_BUFFER_H
#define BASE_MEMORY_TYPED_BUFFER_H

#include "base/macros_magic.h"

#include <QtGlobal>
#include <algorithm>

namespace base {

//
// A scoper for a variable-length structure such as SID, SECURITY_DESCRIPTOR and
// similar. These structures typically consist of a header followed by variable-
// length data, so the size may not match sizeof(T). The class supports
// move-only semantics and typed buffer getters.
//
template <typename T>
class TypedBuffer
{
public:
    TypedBuffer()
        : TypedBuffer(0)
    {
        // Nothing
    }

    // Creates an instance of the object allocating a buffer of the given size.
    explicit TypedBuffer(size_t length)
        : length_(length)
    {
        if (length_ != 0)
            buffer_ = reinterpret_cast<T*>(new quint8[length_]);
    }

    TypedBuffer(TypedBuffer&& rvalue) noexcept
        : TypedBuffer()
    {
        swap(rvalue);
    }

    ~TypedBuffer()
    {
        if (buffer_)
        {
            delete[] reinterpret_cast<quint8*>(buffer_);
            buffer_ = nullptr;
        }
    }

    TypedBuffer& operator=(TypedBuffer&& rvalue) noexcept
    {
        swap(rvalue);
        return *this;
    }

    // Accessors to get the owned buffer.
    // operator* and operator-> will assert() if there is no current buffer.
    T& operator*() const
    {
        assert(buffer_ != nullptr);
        return *buffer_;
    }

    T* operator->() const
    {
        assert(buffer_ != nullptr);
        return buffer_;
    }

    T* get() const { return buffer_; }
    size_t length() const { return length_; }

    // Helper returning a pointer to the structure starting at a specified byte
    // offset.
    T* getAtOffset(quint32 offset)
    {
        return reinterpret_cast<T*>(reinterpret_cast<quint8*>(buffer_) + offset);
    }

    // Allow TypedBuffer<T> to be used in boolean expressions.
    explicit operator bool() const { return buffer_ != nullptr; }

    // Swap two buffers.
    void swap(TypedBuffer& other)
    {
        std::swap(buffer_, other.buffer_);
        std::swap(length_, other.length_);
    }

private:
    // Points to the owned buffer.
    T* buffer_ = nullptr;

    // Length of the owned buffer in bytes.
    size_t length_;

    DISALLOW_COPY_AND_ASSIGN(TypedBuffer);
};

} // namespace base

#endif // BASE_MEMORY_TYPED_BUFFER_H
