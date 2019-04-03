//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__REF_COUNTED_OBJECT_H
#define BASE__REF_COUNTED_OBJECT_H

#include <atomic>
#include <type_traits>
#include <utility>

namespace base {

template <class T>
class RefCountedObject : public T
{
public:
    RefCountedObject() = default;

    template <class P0>
    explicit RefCountedObject(P0&& p0)
        : T(std::forward<P0>(p0))
    {
        // Nothing
    }

    template <class P0, class P1, class... Args>
    RefCountedObject(P0&& p0, P1&& p1, Args&&... args)
        : T(std::forward<P0>(p0),
            std::forward<P1>(p1),
            std::forward<Args>(args)...)
    {
        // Nothing
    }

    virtual void addRef() const { ++ref_count_; }

    virtual void release() const
    {
        --ref_count_;

        if (ref_count_ == 0)
            delete this;
    }

    // Return whether the reference count is one. If the reference count is used
    // in the conventional way, a reference count of 1 implies that the current
    // thread owns the reference and no other thread shares it. This call
    // performs the test for a reference count of one, and performs the memory
    // barrier needed for the owning thread to act on the object, knowing that it
    // has exclusive access to the object.
    virtual bool hasOneRef() const { return ref_count_ == 1; }

protected:
    virtual ~RefCountedObject() = default;

    mutable std::atomic_int ref_count_{ 0 };
};

} // namespace base

#endif // BASE__REF_COUNTED_OBJECT_H
