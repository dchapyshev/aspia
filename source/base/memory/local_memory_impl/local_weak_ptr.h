//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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
// This file forked from Boost.
// Copyright (c) 2001, 2002, 2003 Peter Dimov
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_LOCAL_WEAK_PTR_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_LOCAL_WEAK_PTR_H

#include "base/memory/local_memory_impl/shared_count.h"
#include "base/memory/local_memory_impl/local_shared_ptr.h"

#include <memory>

namespace base {

template <class T>
class local_weak_ptr
{
private:
    // Borland 5.5.1 specific workarounds
    typedef local_weak_ptr<T> this_type;

public:
    typedef typename base::detail::sp_element<T>::type element_type;

    local_weak_ptr() noexcept
        : px(nullptr), pn()
    {
        // Nothing
    }

    //  generated copy constructor, assignment, destructor are fine...

    // ... except in C++0x, move disables the implicit copy

    local_weak_ptr(local_weak_ptr const &r) noexcept
        : px(r.px), pn(r.pn)
    {
        // Nothing
    }

    local_weak_ptr& operator=(local_weak_ptr const &r) noexcept
    {
        px = r.px;
        pn = r.pn;
        return *this;
    }

    //
    //  The "obvious" converting constructor implementation:
    //
    //  template<class Y>
    //  local_weak_ptr(local_weak_ptr<Y> const & r): px(r.px), pn(r.pn) // never throws
    //  {
    //  }
    //
    //  has a serious problem.
    //
    //  r.px may already have been invalidated. The px(r.px)
    //  conversion may require access to *r.px (virtual inheritance).
    //
    //  It is not possible to avoid spurious access violations since
    //  in multithreaded programs r.px may be invalidated at any point.
    //

    template <class Y>
    local_weak_ptr(local_weak_ptr<Y> const& r,
        typename base::detail::sp_enable_if_convertible<Y, T>::type = base::detail::sp_empty()) noexcept
        : px(r.lock().get()), pn(r.pn)
    {
        base::detail::sp_assert_convertible<Y, T>();
    }

    template <class Y>
    local_weak_ptr(local_weak_ptr<Y>&& r,
        typename base::detail::sp_enable_if_convertible<Y, T>::type = base::detail::sp_empty()) noexcept
        : px(r.lock().get()),
          pn(static_cast<base::detail::weak_count&&>(r.pn))
    {
        base::detail::sp_assert_convertible<Y, T>();
        r.px = nullptr;
    }

    // for better efficiency in the T == Y case
    local_weak_ptr(local_weak_ptr &&r) noexcept
        : px(r.px), pn(static_cast<base::detail::weak_count &&>(r.pn))
    {
        r.px = nullptr;
    }

    // for better efficiency in the T == Y case
    local_weak_ptr &operator=(local_weak_ptr&& r) noexcept
    {
        this_type(static_cast<local_weak_ptr&&>(r)).swap(*this);
        return *this;
    }

    template <class Y>
    local_weak_ptr(local_shared_ptr<Y> const& r,
        typename base::detail::sp_enable_if_convertible<Y, T>::type = base::detail::sp_empty()) noexcept
        : px(r.px), pn(r.pn)
    {
        base::detail::sp_assert_convertible<Y, T>();
    }

    template <class Y>
    local_weak_ptr &operator=(local_weak_ptr<Y> const& r) noexcept
    {
        base::detail::sp_assert_convertible<Y, T>();

        px = r.lock().get();
        pn = r.pn;

        return *this;
    }

    template <class Y>
    local_weak_ptr &operator=(local_weak_ptr<Y>&& r) noexcept
    {
        this_type(static_cast<local_weak_ptr<Y>&&>(r)).swap(*this);
        return *this;
    }

    template <class Y>
    local_weak_ptr &operator=(local_shared_ptr<Y> const& r) noexcept
    {
        base::detail::sp_assert_convertible<Y, T>();

        px = r.px;
        pn = r.pn;

        return *this;
    }

    local_shared_ptr<T> lock() const noexcept
    {
        return local_shared_ptr<T>(*this, base::detail::sp_nothrow_tag());
    }

    long use_count() const noexcept { return pn.use_count(); }
    bool expired() const noexcept { return pn.use_count() == 0; }

    // extension, not in std::local_weak_ptr
    bool _empty() const { return pn.empty(); }

    void reset() noexcept { this_type().swap(*this); }

    void swap(this_type &other) noexcept
    {
        std::swap(px, other.px);
        pn.swap(other.pn);
    }

    template <typename Y>
    void _internal_aliasing_assign(local_weak_ptr<Y> const &r, element_type *px2)
    {
        px = px2;
        pn = r.pn;
    }

    template <class Y>
    bool owner_before(local_weak_ptr<Y> const &rhs) const noexcept { return pn < rhs.pn; }

    template <class Y>
    bool owner_before(local_shared_ptr<Y> const &rhs) const noexcept { return pn < rhs.pn; }

    // Tasteless as this may seem, making all members public allows member templates
    // to work in the absence of member template friends. (Matthew Langston)

private:
    template <class Y>
    friend class local_weak_ptr;
    template <class Y>
    friend class local_shared_ptr;

    element_type *px; // contained pointer
    base::detail::weak_count pn; // reference counter

}; // local_weak_ptr

template <class T, class U>
inline bool operator<(local_weak_ptr<T> const& a, local_weak_ptr<U> const& b) noexcept
{
    return a.owner_before(b);
}

template <class T>
void swap(local_weak_ptr<T>& a, local_weak_ptr<T>& b) noexcept
{
    a.swap(b);
}

} // namespace base

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_LOCAL_WEAK_PTR_H
