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
// Copyright (c) 2001, 2002, 2003 Peter Dimov and Multi Media Ltd.
// Copyright 2004-2005 Peter Dimov
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_COUNTED_IMPL_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_COUNTED_IMPL_H

#include "base/memory/local_memory_impl/checked_delete.h"
#include "base/memory/local_memory_impl/sp_counted_base_nt.h"

#include <cstddef>
#include <typeinfo>
#include <memory>

namespace base::detail {

template <class X>
class sp_counted_impl_p : public sp_counted_base
{
private:
    X* px_;

    sp_counted_impl_p(sp_counted_impl_p const&);
    sp_counted_impl_p &operator=(sp_counted_impl_p const&);

    typedef sp_counted_impl_p<X> this_type;

public:
    explicit sp_counted_impl_p(X* px)
        : px_(px)
    {
        // Nothing
    }

    virtual void dispose() // nothrow
    {
        base::checked_delete(px_);
    }

    virtual void* get_deleter(std::type_info const&) { return nullptr; }
    virtual void* get_untyped_deleter() { return nullptr; }
};

template <class P, class D>
class sp_counted_impl_pd : public sp_counted_base
{
private:
    P ptr; // copy constructor must not throw
    D del; // copy constructor must not throw

    sp_counted_impl_pd(sp_counted_impl_pd const&);
    sp_counted_impl_pd &operator=(sp_counted_impl_pd const&);

    typedef sp_counted_impl_pd<P, D> this_type;

public:
    // pre: d(p) must not throw

    sp_counted_impl_pd(P p, D &d)
        : ptr(p), del(d)
    {
        // Nothing
    }

    sp_counted_impl_pd(P p)
        : ptr(p), del()
    {
        // Nothing
    }

    virtual void dispose() // nothrow
    {
        del(ptr);
    }

    virtual void* get_deleter(std::type_info const& ti)
    {
        return ti == typeid(D) ? &reinterpret_cast<char&>(del) : nullptr;
    }

    virtual void* get_untyped_deleter()
    {
        return &reinterpret_cast<char&>(del);
    }
};

template <class P, class D, class A>
class sp_counted_impl_pda : public sp_counted_base
{
private:
    P p_; // copy constructor must not throw
    D d_; // copy constructor must not throw
    A a_; // copy constructor must not throw

    sp_counted_impl_pda(sp_counted_impl_pda const&);
    sp_counted_impl_pda &operator=(sp_counted_impl_pda const&);

    typedef sp_counted_impl_pda<P, D, A> this_type;

public:
    // pre: d( p ) must not throw

    sp_counted_impl_pda(P p, D& d, A a)
        : p_(p), d_(d), a_(a)
    {
        // Nothing
    }

    sp_counted_impl_pda(P p, A a)
        : p_(p), d_(a), a_(a)
    {
        // Nothing
    }

    virtual void dispose() // nothrow
    {
        d_(p_);
    }

    virtual void destroy() // nothrow
    {
        typedef typename std::allocator_traits<A>::template rebind_alloc<this_type> A2;

        A2 a2(a_);
        std::allocator_traits<A2>::destroy(a2, this);
        a2.deallocate(this, 1);
    }

    virtual void* get_deleter(std::type_info const& ti)
    {
        return ti == typeid(D) ? &reinterpret_cast<char&>(d_) : nullptr;
    }

    virtual void* get_untyped_deleter()
    {
        return &reinterpret_cast<char&>(d_);
    }
};

} // namespace base::detail

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_COUNTED_IMPL_H
