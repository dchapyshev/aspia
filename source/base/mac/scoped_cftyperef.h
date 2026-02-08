//
// SmartCafe Project
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

#ifndef BASE_MAC_SCOPED_CFTYPEREF_H
#define BASE_MAC_SCOPED_CFTYPEREF_H

#include "base/logging.h"

#include <CoreFoundation/CoreFoundation.h>

namespace base {

// RETAIN: ScopedTypeRef should retain the object when it takes ownership.
// ASSUME: Assume the object already has already been retained. ScopedTypeRef takes over ownership.
enum class RetainPolicy { RETAIN, ASSUME };

namespace internal {

template <typename T>
struct CFTypeRefTraits
{
    static T invalidValue() { return nullptr; }
    static void release(T ref) { CFRelease(ref); }
    static T retain(T ref)
    {
        CFRetain(ref);
        return ref;
    }
};

template <typename T, typename Traits>
class ScopedTypeRef
{
public:
    ScopedTypeRef()
        : ptr_(Traits::invalidValue())
    {
        // Nothing
    }

    explicit ScopedTypeRef(T ptr)
        : ptr_(ptr)
    {
        // Nothing
    }

    ScopedTypeRef(T ptr, RetainPolicy policy)
        : ScopedTypeRef(ptr)
    {
        if (ptr_ && policy == RetainPolicy::RETAIN)
            Traits::retain(ptr_);
    }

    ScopedTypeRef(const ScopedTypeRef<T, Traits>& rhs)
        : ptr_(rhs.ptr_)
    {
        if (ptr_)
            ptr_ = Traits::retain(ptr_);
    }

    ~ScopedTypeRef()
    {
        if (ptr_)
            Traits::release(ptr_);
    }

    T get() const { return ptr_; }
    T operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_; }

    bool operator!() const { return !ptr_; }

    ScopedTypeRef& operator=(const T& rhs)
    {
        if (ptr_)
            Traits::release(ptr_);
        ptr_ = rhs;
        return *this;
    }

    ScopedTypeRef& operator=(const ScopedTypeRef<T, Traits>& rhs)
    {
        reset(rhs.get(), RetainPolicy::RETAIN);
        return *this;
    }

    // This is intended to take ownership of objects that are created by pass-by-pointer initializers.
    T* initializeInto()
    {
        DCHECK(!ptr_);
        return &ptr_;
    }

    void reset(T ptr, RetainPolicy policy = RetainPolicy::ASSUME)
    {
        if (ptr && policy == RetainPolicy::RETAIN)
            Traits::retain(ptr);
        if (ptr_)
            Traits::release(ptr_);
        ptr_ = ptr;
    }

    T release()
    {
        T temp = ptr_;
        ptr_ = Traits::invalidValue();
        return temp;
    }

private:
    T ptr_;
};

} // namespace internal

template <typename T>
using ScopedCFTypeRef = internal::ScopedTypeRef<T, internal::CFTypeRefTraits<T>>;

template <typename T>
static ScopedCFTypeRef<T> adoptCF(T cftype)
{
    return ScopedCFTypeRef<T>(cftype, RetainPolicy::RETAIN);
}

template <typename T>
static ScopedCFTypeRef<T> scopedCF(T cftype)
{
    return ScopedCFTypeRef<T>(cftype);
}

} // namespace base

#endif // BASE_MAC_SCOPED_CFTYPEREF_H
