//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
// Copyright (c) Greg Colvin and Beman Dawes 1998, 1999.
// Copyright (c) 2002, 2003 Peter Dimov
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_LOCAL_SHARED_PTR_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_LOCAL_SHARED_PTR_H

#include "base/memory/local_memory_impl/checked_delete.h"
#include "base/memory/local_memory_impl/shared_count.h"
#include "base/memory/local_memory_impl/sp_convertible.h"

#include <cassert>
#include <memory>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <typeinfo>
#include <iosfwd>

namespace base {

template <class T>
class local_shared_ptr;
template <class T>
class local_weak_ptr;
template <class T>
class enable_shared_from_this;
class enable_shared_from_raw;

namespace detail {

// sp_element, element_type

template <class T>
struct sp_element { typedef T type; };

template <class T>
struct sp_element<T[]> { typedef T type; };

template <class T, std::size_t N>
struct sp_element<T[N]> { typedef T type; };

// sp_dereference, return type of operator*

template <class T>
struct sp_dereference { typedef T &type; };

template <>
struct sp_dereference<void> { typedef void type; };

template <>
struct sp_dereference<void const> { typedef void type; };

template <>
struct sp_dereference<void volatile> { typedef void type; };

template <>
struct sp_dereference<void const volatile> { typedef void type; };

template <class T>
struct sp_dereference<T[]> { typedef void type; };

template <class T, std::size_t N>
struct sp_dereference<T[N]> { typedef void type; };

// sp_member_access, return type of operator->

template <class T>
struct sp_member_access { typedef T *type; };

template <class T>
struct sp_member_access<T[]> { typedef void type; };

template <class T, std::size_t N>
struct sp_member_access<T[N]> { typedef void type; };

// enable_shared_from_this support

template <class X, class Y, class T>
inline void sp_enable_shared_from_this(
    base::local_shared_ptr<X> const* ppx, Y const* py, base::enable_shared_from_this<T> const* pe)
{
    if (pe != nullptr)
    {
        pe->_internal_accept_owner(ppx, const_cast<Y*>(py));
    }
}

template <class X, class Y>
inline void sp_enable_shared_from_this(
    base::local_shared_ptr<X>* ppx, Y const *py, base::enable_shared_from_raw const* pe);

inline void sp_enable_shared_from_this(...)
{
    // Nothing
}

// sp_assert_convertible

template <class Y, class T>
inline void sp_assert_convertible()
{
    // static_assert( sp_convertible< Y, T >::value );
    typedef char tmp[sp_convertible<Y, T>::value ? 1 : -1];
    (void)sizeof(tmp);
}

// pointer constructor helper

template <class T, class Y>
inline void sp_pointer_construct(base::local_shared_ptr<T>* ppx, Y* p, base::detail::shared_count& pn)
{
    base::detail::shared_count(p).swap(pn);
    base::detail::sp_enable_shared_from_this(ppx, p, p);
}

template <class T, class Y>
inline void sp_pointer_construct(
    base::local_shared_ptr<T[]>* /*ppx*/, Y* p, base::detail::shared_count& pn)
{
    sp_assert_convertible<Y[], T[]>();
    base::detail::shared_count(p, base::checked_array_deleter<T>()).swap(pn);
}

template <class T, std::size_t N, class Y>
inline void sp_pointer_construct(
    base::local_shared_ptr<T[N]>* /*ppx*/, Y* p,  base::detail::shared_count& pn)
{
    sp_assert_convertible<Y[N], T[N]>();
    base::detail::shared_count(p, base::checked_array_deleter<T>()).swap(pn);
}

// deleter constructor helper

template <class T, class Y>
inline void sp_deleter_construct(base::local_shared_ptr<T>* ppx, Y* p)
{
    base::detail::sp_enable_shared_from_this(ppx, p, p);
}

template <class T, class Y>
inline void sp_deleter_construct(base::local_shared_ptr<T[]>* /*ppx*/, Y* /*p*/)
{
    sp_assert_convertible<Y[], T[]>();
}

template <class T, std::size_t N, class Y>
inline void sp_deleter_construct(base::local_shared_ptr<T[N]>* /*ppx*/, Y* /*p*/)
{
    sp_assert_convertible<Y[N], T[N]>();
}

} // namespace detail

//
//  local_shared_ptr
//
//  An enhanced relative of scoped_ptr with reference counted copy semantics.
//  The object pointed to is deleted when the last local_shared_ptr pointing to it
//  is destroyed or reset.
//

template <class T>
class local_shared_ptr
{
private:
    // Borland 5.5.1 specific workaround
    typedef local_shared_ptr<T> this_type;

public:
    typedef typename base::detail::sp_element<T>::type element_type;

    local_shared_ptr() noexcept
        : px(nullptr), pn()
    {
        // Nothing
    }

    local_shared_ptr(std::nullptr_t) noexcept
        : px(nullptr), pn()
    {
        // Nothing
    }

    template <class Y>
    explicit local_shared_ptr(Y* p)
        : px(p), pn() // Y must be complete
    {
        base::detail::sp_pointer_construct(this, p, pn);
    }

    //
    // Requirements: D's copy constructor must not throw
    //
    // local_shared_ptr will release p by calling d(p)
    //

    template <class Y, class D>
    local_shared_ptr(Y* p, D d)
        : px(p), pn(p, d)
    {
        base::detail::sp_deleter_construct(this, p);
    }

    template <class D>
    local_shared_ptr(std::nullptr_t p, D d)
        : px(p), pn(p, d)
    {
    }

    // As above, but with allocator. A's copy constructor shall not throw.

    template <class Y, class D, class A>
    local_shared_ptr(Y* p, D d, A a)
        : px(p), pn(p, d, a)
    {
        base::detail::sp_deleter_construct(this, p);
    }

    template <class D, class A>
    local_shared_ptr(std::nullptr_t p, D d, A a)
        : px(p), pn(p, d, a)
    {
        // Nothing
    }

    //  generated copy constructor, destructor are fine...

    // ... except in C++0x, move disables the implicit copy

    local_shared_ptr(local_shared_ptr const& r) noexcept
        : px(r.px), pn(r.pn)
    {
        // Nothing
    }

    template <class Y>
    explicit local_shared_ptr(local_weak_ptr<Y> const& r)
        : pn(r.pn) // may throw
    {
        base::detail::sp_assert_convertible<Y, T>();

        // it is now safe to copy r.px, as pn(r.pn) did not throw
        px = r.px;
    }

    template <class Y>
    local_shared_ptr(local_weak_ptr<Y> const& r, base::detail::sp_nothrow_tag) noexcept
        : px(0), pn(r.pn, base::detail::sp_nothrow_tag())
    {
        if (!pn.empty())
            px = r.px;
    }

    template <class Y>
    local_shared_ptr(local_shared_ptr<Y> const& r,
        typename base::detail::sp_enable_if_convertible<Y, T>::type = base::detail::sp_empty()) noexcept
        : px(r.px), pn(r.pn)
    {
        base::detail::sp_assert_convertible<Y, T>();
    }

    // aliasing
    template <class Y>
    local_shared_ptr(local_shared_ptr<Y> const& r, element_type* p) noexcept
        : px(p), pn(r.pn)
    {
        // Nothing
    }

    template <class Y, class D>
    local_shared_ptr(std::unique_ptr<Y, D>&& r)
        : px(r.get()), pn()
    {
        base::detail::sp_assert_convertible<Y, T>();

        typename std::unique_ptr<Y, D>::pointer tmp = r.get();
        pn = base::detail::shared_count(r);

        base::detail::sp_deleter_construct(this, tmp);
    }

    // assignment

    local_shared_ptr& operator=(local_shared_ptr const& r) noexcept
    {
        this_type(r).swap(*this);
        return *this;
    }

    template <class Y>
    local_shared_ptr& operator=(local_shared_ptr<Y> const& r) noexcept
    {
        this_type(r).swap(*this);
        return *this;
    }

    template <class Y, class D>
    local_shared_ptr& operator=(std::unique_ptr<Y, D>&& r)
    {
        this_type(static_cast<std::unique_ptr<Y, D> &&>(r)).swap(*this);
        return *this;
    }

    // Move support

    local_shared_ptr(local_shared_ptr&& r) noexcept
        : px(r.px), pn()
    {
        pn.swap(r.pn);
        r.px = nullptr;
    }

    template <class Y>
    local_shared_ptr(local_shared_ptr<Y>&& r,
        typename base::detail::sp_enable_if_convertible<Y, T>::type = base::detail::sp_empty()) noexcept
        : px(r.px), pn()
    {
        base::detail::sp_assert_convertible<Y, T>();

        pn.swap(r.pn);
        r.px = nullptr;
    }

    local_shared_ptr &operator=(local_shared_ptr&& r) noexcept
    {
        this_type(static_cast<local_shared_ptr&&>(r)).swap(*this);
        return *this;
    }

    template <class Y>
    local_shared_ptr &operator=(local_shared_ptr<Y>&& r) noexcept
    {
        this_type(static_cast<local_shared_ptr<Y>&&>(r)).swap(*this);
        return *this;
    }

    local_shared_ptr &operator=(std::nullptr_t) noexcept // never throws
    {
        this_type().swap(*this);
        return *this;
    }

    void reset() noexcept // never throws in 1.30+
    {
        this_type().swap(*this);
    }

    template <class Y>
    void reset(Y* p) // Y must be complete
    {
        assert(p == nullptr || p != px); // catch self-reset errors
        this_type(p).swap(*this);
    }

    template <class Y, class D>
    void reset(Y* p, D d)
    {
        this_type(p, d).swap(*this);
    }

    template <class Y, class D, class A>
    void reset(Y* p, D d, A a)
    {
        this_type(p, d, a).swap(*this);
    }

    template <class Y>
    void reset(local_shared_ptr<Y> const& r, element_type* p)
    {
        this_type(r, p).swap(*this);
    }

    // never throws (but has a KIT_ASSERT in it, so not marked with noexcept)
    typename base::detail::sp_dereference<T>::type operator*() const
    {
        assert(px != nullptr);
        return *px;
    }

    // never throws (but has a KIT_ASSERT in it, so not marked with noexcept)
    typename base::detail::sp_member_access<T>::type operator->() const
    {
        assert(px != nullptr);
        return px;
    }

    element_type* get() const noexcept { return px; }

    // implicit conversion to "bool"
    explicit operator bool() const noexcept { return px != nullptr; }
    bool operator!() const noexcept { return px == nullptr; }
    bool unique() const noexcept { return pn.unique(); }
    long use_count() const noexcept { return pn.use_count(); }

    void swap(local_shared_ptr& other) noexcept
    {
        std::swap(px, other.px);
        pn.swap(other.pn);
    }

    template <class Y>
    bool owner_before(local_shared_ptr<Y> const& rhs) const noexcept
    {
        return pn < rhs.pn;
    }

    template <class Y>
    bool owner_before(local_weak_ptr<Y> const& rhs) const noexcept
    {
        return pn < rhs.pn;
    }

    void* _internal_get_deleter(std::type_info const& ti) const noexcept
    {
        return pn.get_deleter(ti);
    }

    void* _internal_get_untyped_deleter() const noexcept
    {
        return pn.get_untyped_deleter();
    }

    bool _internal_equiv(local_shared_ptr const& r) const noexcept
    {
        return px == r.px && pn == r.pn;
    }

    // Tasteless as this may seem, making all members public allows member templates
    // to work in the absence of member template friends. (Matthew Langston)

private:
    template <class Y>
    friend class local_shared_ptr;
    template <class Y>
    friend class local_weak_ptr;

    element_type* px; // contained pointer
    base::detail::shared_count pn; // reference counter

}; // local_shared_ptr

template <class T, class U>
inline bool operator==(local_shared_ptr<T> const& a, local_shared_ptr<U> const& b) noexcept
{
    return a.get() == b.get();
}

template <class T, class U>
inline bool operator!=(local_shared_ptr<T> const& a, local_shared_ptr<U> const& b) noexcept
{
    return a.get() != b.get();
}

template <class T>
inline bool operator==(local_shared_ptr<T> const& p, std::nullptr_t) noexcept
{
    return p.get() == nullptr;
}

template <class T>
inline bool operator==(std::nullptr_t, local_shared_ptr<T> const& p) noexcept
{
    return p.get() == nullptr;
}

template <class T>
inline bool operator!=(local_shared_ptr<T> const& p, std::nullptr_t) noexcept
{
    return p.get() != nullptr;
}

template <class T>
inline bool operator!=(std::nullptr_t, local_shared_ptr<T> const& p) noexcept
{
    return p.get() != nullptr;
}

template <class T, class U>
inline bool operator<(local_shared_ptr<T> const& a, local_shared_ptr<U> const& b) noexcept
{
    return a.owner_before(b);
}

template <class T>
inline void swap(local_shared_ptr<T>& a, local_shared_ptr<T>& b) noexcept
{
    a.swap(b);
}

template <class T, class U>
local_shared_ptr<T> static_pointer_cast(local_shared_ptr<U> const& r) noexcept
{
    (void)static_cast<T*>(static_cast<U*>(nullptr));

    typedef typename local_shared_ptr<T>::element_type E;

    E* p = static_cast<E*>(r.get());
    return local_shared_ptr<T>(r, p);
}

template <class T, class U>
local_shared_ptr<T> const_pointer_cast(local_shared_ptr<U> const& r) noexcept
{
    (void)const_cast<T*>(static_cast<U*>(nullptr));

    typedef typename local_shared_ptr<T>::element_type E;

    E* p = const_cast<E*>(r.get());
    return local_shared_ptr<T>(r, p);
}

template <class T, class U>
local_shared_ptr<T> dynamic_pointer_cast(local_shared_ptr<U> const& r) noexcept
{
    (void)dynamic_cast<T*>(static_cast<U*>(nullptr));

    typedef typename local_shared_ptr<T>::element_type E;

    E* p = dynamic_cast<E*>(r.get());
    return p ? local_shared_ptr<T>(r, p) : local_shared_ptr<T>();
}

template <class T, class U>
local_shared_ptr<T> reinterpret_pointer_cast(local_shared_ptr<U> const& r) noexcept
{
    (void)reinterpret_cast<T*>(static_cast<U*>(nullptr));

    typedef typename local_shared_ptr<T>::element_type E;

    E* p = reinterpret_cast<E*>(r.get());
    return local_shared_ptr<T>(r, p);
}

// get_pointer() enables base::mem_fn to recognize local_shared_ptr

template <class T>
inline typename local_shared_ptr<T>::element_type* get_pointer(local_shared_ptr<T> const& p) noexcept
{
    return p.get();
}

// operator<<

template <class E, class T, class Y>
std::basic_ostream<E, T>& operator<<(std::basic_ostream<E, T>& os, local_shared_ptr<Y> const& p)
{
    os << p.get();
    return os;
}

// get_deleter

namespace detail {

template <class D, class T>
D* basic_get_deleter(local_shared_ptr<T> const& p) noexcept
{
    return static_cast<D*>(p._internal_get_deleter(typeid(D)));
}

class esft2_deleter_wrapper
{
private:
    local_shared_ptr<void const volatile> deleter_;

public:
      esft2_deleter_wrapper() = default;

      template <class T>
      void set_deleter(local_shared_ptr<T> const& deleter)
      {
            deleter_ = deleter;
      }

      template <typename D>
      D* get_deleter() const noexcept
      {
            return base::detail::basic_get_deleter<D>(deleter_);
      }

      template <class T>
      void operator()(T*)
      {
            assert(deleter_.use_count() <= 1);
            deleter_.reset();
      }
};

} // namespace detail

template <class D, class T>
D* get_deleter(local_shared_ptr<T> const& p) noexcept
{
    D* del = base::detail::basic_get_deleter<D>(p);
    if (del == nullptr)
    {
        base::detail::esft2_deleter_wrapper* del_wrapper =
            base::detail::basic_get_deleter<base::detail::esft2_deleter_wrapper>(p);
        // The following get_deleter method call is fully qualified because
        // older versions of gcc (2.95, 3.2.3) fail to compile it when written
        // del_wrapper->get_deleter<D>()
        if (del_wrapper)
            del = del_wrapper->::base::detail::esft2_deleter_wrapper::get_deleter<D>();
    }

    return del;
}

// hash_value

template <class T>
struct hash;

template <class T>
std::size_t hash_value(base::local_shared_ptr<T> const& p) noexcept
{
    return base::hash<T*>()(p.get());
}

} // namespace base

namespace std {

template <class T>
struct hash<base::local_shared_ptr<T>>
{
    inline size_t operator()(const base::local_shared_ptr<T>& p) const noexcept
    {
        return hash<T*>()(p.get());
    }
};

} // namespace std

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_LOCAL_SHARED_PTR_H
