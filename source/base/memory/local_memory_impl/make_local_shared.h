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
// Copyright (c) 2007, 2008, 2012 Peter Dimov
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_MAKE_LOCAL_SHARED_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_MAKE_LOCAL_SHARED_H

#include "base/memory/local_memory_impl/local_shared_ptr.h"

#include <cstddef>
#include <new>

namespace base {

namespace detail {

template <class T>
class sp_ms_deleter
{
private:
    typedef typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type storage_type;

    bool initialized_;
    storage_type storage_;

private:
    void destroy()
    {
        if (initialized_)
        {
            // fixes incorrect aliasing warning
            T* p = reinterpret_cast<T*>(&storage_);
            p->~T();

            initialized_ = false;
        }
    }

public:
    sp_ms_deleter() noexcept
        : initialized_(false)
    {
        // Nothing
    }

    template <class A>
    explicit sp_ms_deleter(A const&) noexcept
        : initialized_(false)
    {
        // Nothing
    }

    // optimization: do not copy storage_
    sp_ms_deleter(sp_ms_deleter const&) noexcept
        : initialized_(false)
    {
        // Nothing
    }

    ~sp_ms_deleter() { destroy(); }

    void operator()(T*) { destroy(); }

    static void operator_fn(T*) // operator() can't be static
    {
        // Nothing
    }

    void* address() noexcept { return &storage_; }

    void set_initialized() noexcept { initialized_ = true; }
};

template <class T, class A>
class sp_as_deleter
{
private:
    typedef typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type storage_type;

    storage_type storage_;
    A a_;
    bool initialized_;

private:
    void destroy()
    {
        if (initialized_)
        {
            T *p = reinterpret_cast<T *>(&storage_);
            std::allocator_traits<A>::destroy(a_, p);
            initialized_ = false;
        }
    }

public:
    sp_as_deleter(A const& a) noexcept
        : a_(a), initialized_(false)
    {
        // Nothing
    }

    // optimization: do not copy storage_
    sp_as_deleter(sp_as_deleter const& r) noexcept
        : a_(r.a_), initialized_(false)
    {
        // Nothing
    }

    ~sp_as_deleter() { destroy(); }

    void operator()(T*) { destroy(); }

    static void operator_fn(T*) // operator() can't be static
    {
        // Nothing
    }

    void* address() noexcept { return &storage_; }

    void set_initialized() noexcept { initialized_ = true; }
};

template <class T>
struct sp_if_not_array { typedef base::local_shared_ptr<T> type; };

template <class T>
struct sp_if_not_array<T[]> {};

template <class T, std::size_t N>
struct sp_if_not_array<T[N]> {};

template <class T>
T&& sp_forward(T& t) noexcept
{
    return static_cast<T&&>(t);
}

} // namespace detail

// _noinit versions

template <class T>
typename base::detail::sp_if_not_array<T>::type make_local_shared_noinit()
{
    base::local_shared_ptr<T> pt(static_cast<T*>(0),
        base::detail::sp_inplace_tag<base::detail::sp_ms_deleter<T>>());

    base::detail::sp_ms_deleter<T>* pd =
        static_cast<base::detail::sp_ms_deleter<T> *>(pt._internal_get_untyped_deleter());

    void *pv = pd->address();

    ::new (pv) T;
    pd->set_initialized();

    T* pt2 = static_cast<T*>(pv);

    base::detail::sp_enable_shared_from_this(&pt, pt2, pt2);
    return base::local_shared_ptr<T>(pt, pt2);
}

template <class T, class A>
typename base::detail::sp_if_not_array<T>::type allocate_shared_noinit(A const& a)
{
    base::local_shared_ptr<T> pt(static_cast<T*>(0),
        base::detail::sp_inplace_tag<base::detail::sp_ms_deleter<T>>(), a);

    base::detail::sp_ms_deleter<T>* pd =
        static_cast<base::detail::sp_ms_deleter<T>*>(pt._internal_get_untyped_deleter());

    void* pv = pd->address();

    ::new (pv) T;
    pd->set_initialized();

    T* pt2 = static_cast<T*>(pv);

    base::detail::sp_enable_shared_from_this(&pt, pt2, pt2);
    return base::local_shared_ptr<T>(pt, pt2);
}

// Variadic templates, rvalue reference

template <class T, class... Args>
typename base::detail::sp_if_not_array<T>::type make_local_shared(Args&&... args)
{
    base::local_shared_ptr<T> pt(static_cast<T*>(0),
        base::detail::sp_inplace_tag<base::detail::sp_ms_deleter<T>>());

    base::detail::sp_ms_deleter<T>* pd =
        static_cast<base::detail::sp_ms_deleter<T>*>(pt._internal_get_untyped_deleter());

    void* pv = pd->address();

    ::new (pv) T(base::detail::sp_forward<Args>(args)...);
    pd->set_initialized();

    T* pt2 = static_cast<T*>(pv);

    base::detail::sp_enable_shared_from_this(&pt, pt2, pt2);
    return base::local_shared_ptr<T>(pt, pt2);
}

template <class T, class A, class... Args>
typename base::detail::sp_if_not_array<T>::type allocate_shared(A const& a, Args&&... args)
{
    typedef typename std::allocator_traits<A>::template rebind_alloc<T> A2;
    A2 a2(a);

    typedef base::detail::sp_as_deleter<T, A2> D;

    base::local_shared_ptr<T> pt(static_cast<T*>(0), base::detail::sp_inplace_tag<D>(), a2);

    D* pd = static_cast<D*>(pt._internal_get_untyped_deleter());
    void* pv = pd->address();

    std::allocator_traits<A2>::construct(a2, static_cast<T*>(pv),
                                         base::detail::sp_forward<Args>(args)...);

    pd->set_initialized();

    T* pt2 = static_cast<T*>(pv);

    base::detail::sp_enable_shared_from_this(&pt, pt2, pt2);
    return base::local_shared_ptr<T>(pt, pt2);
}

} // namespace base

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_MAKE_LOCAL_SHARED_H
