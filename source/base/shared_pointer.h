//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_SHARED_POINTER_H
#define BASE_SHARED_POINTER_H

#include <utility>

namespace base {

template<typename T>
class SharedPointer
{
public:
    SharedPointer()
        : ptr_(nullptr),
          ref_count_(nullptr)
    {
        // Nothing
    }

    SharedPointer(T* ptr)
        : ptr_(ptr),
          ref_count_(new int(1))
    {
        // Nothing
    }

    SharedPointer(const SharedPointer& other)
        : ptr_(other.ptr_),
          ref_count_(other.ref_count_)
    {
        if (ref_count_)
            ++(*ref_count_);
    }

    SharedPointer& operator=(const SharedPointer& other)
    {
        if (this != &other)
        {
            release();

            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;

            if (ref_count_)
                ++(*ref_count_);
        }

        return *this;
    }

    SharedPointer(SharedPointer&& other) noexcept
        : ptr_(other.ptr_),
          ref_count_(other.ref_count_)
    {
        other.ptr_ = nullptr;
        other.ref_count_ = nullptr;
    }

    SharedPointer& operator=(SharedPointer&& other) noexcept
    {
        if (this != &other)
        {
            release();

            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;

            other.ptr_ = nullptr;
            other.ref_count_ = nullptr;
        }

        return *this;
    }

    ~SharedPointer()
    {
        release();
    }

    int useCount() const
    {
        return ref_count_ ? *ref_count_ : 0;
    }

    bool isUnique() const { return useCount() == 1; }
    bool isEmpty() const { return !ptr_; }

    void reset(T* ptr = nullptr)
    {
        release();

        if (ptr)
        {
            ptr_ = ptr;
            ref_count_ = new int(1);
        }
        else
        {
            ptr_ = nullptr;
            ref_count_ = nullptr;
        }
    }

    void swap(SharedPointer& other)
    {
        std::swap(ptr_, other.ptr_);
        std::swap(ref_count_, other.ref_count_);
    }

    T* get() { return ptr_; }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    operator bool() const noexcept { return ptr_; }

private:
    void release()
    {
        if (!ref_count_)
            return;

        --(*ref_count_);

        if (*ref_count_ == 0)
        {
            delete ptr_;
            delete ref_count_;
        }
    }

    T* ptr_;
    int* ref_count_;
};

} // namespace base

#endif // BASE_SHARED_POINTER_H
