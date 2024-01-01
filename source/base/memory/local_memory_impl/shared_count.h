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
// Copyright (c) 2001, 2002, 2003 Peter Dimov and Multi Media Ltd.
// Copyright 2004-2005 Peter Dimov
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_SHARED_COUNT_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_SHARED_COUNT_H

#include "base/memory/local_memory_impl/checked_delete.h"
#include "base/memory/local_memory_impl/bad_local_weak_ptr.h"
#include "base/memory/local_memory_impl/sp_counted_base_nt.h"
#include "base/memory/local_memory_impl/sp_counted_impl.h"

#include <functional>
#include <memory>

namespace base::detail {

struct sp_nothrow_tag {};

template <class D>
struct sp_inplace_tag {};

template <class T>
class sp_reference_wrapper
{
public:
    explicit sp_reference_wrapper(T &t)
        : t_(std::addressof(t))
    {
        // Nothing
    }

    template <class Y>
    void operator()(Y *p) const
    {
        (*t_)(p);
    }

private:
    T *t_;
};

template <class D>
struct sp_convert_reference { typedef D type; };

template <class D>
struct sp_convert_reference<D &> { typedef sp_reference_wrapper<D> type; };

class weak_count;

class shared_count
{
private:
    sp_counted_base* pi_;

    friend class weak_count;

public:
    shared_count()
        : pi_(nullptr) // nothrow
    {
        // Nothing
    }

    template <class Y>
    explicit shared_count(Y *p)
        : pi_(nullptr)
    {
        try
        {
            pi_ = new sp_counted_impl_p<Y>(p);
        }
        catch (...)
        {
            base::checked_delete(p);
            throw;
        }
    }

    template <class P, class D>
    shared_count(P p, D d)
        : pi_(nullptr)
    {
        try
        {
            pi_ = new sp_counted_impl_pd<P, D>(p, d);
        }
        catch (...)
        {
            d(p); // delete p
            throw;
        }
    }

    template <class P, class D>
    shared_count(P p, sp_inplace_tag<D>)
        : pi_(nullptr)
    {
        try
        {
            pi_ = new sp_counted_impl_pd<P, D>(p);
        } catch (...)
        {
            D::operator_fn(p); // delete p
            throw;
        }
    }

    template <class P, class D, class A>
    shared_count(P p, D d, A a)
        : pi_(nullptr)
    {
        typedef sp_counted_impl_pda<P, D, A> impl_type;
        typedef typename std::allocator_traits<A>::template rebind_alloc<impl_type> A2;

        A2 a2(a);

        try
        {
            impl_type* pi = std::allocator_traits<A2>::allocate(a2, 1);
            pi_ = pi;
            std::allocator_traits<A2>::construct(a2, pi, p, d, a);
        }
        catch (...)
        {
            d(p);

            if (pi_ != nullptr)
            {
                a2.deallocate(static_cast<impl_type*>(pi_), 1);
            }

            throw;
        }
    }

    template <class P, class D, class A>
    shared_count(P p, sp_inplace_tag<D>, A a)
        : pi_(nullptr)
    {
        typedef sp_counted_impl_pda<P, D, A> impl_type;
        typedef typename std::allocator_traits<A>::template rebind_alloc<impl_type> A2;

        A2 a2(a);

        try
        {
            impl_type* pi = std::allocator_traits<A2>::allocate(a2, 1);
            pi_ = pi;
            std::allocator_traits<A2>::construct(a2, pi, p, a);
        }
        catch (...)
        {
            D::operator_fn(p);

            if (pi_ != nullptr)
            {
                a2.deallocate(static_cast<impl_type*>(pi_), 1);
            }

            throw;
        }
    }

    template <class Y, class D>
    explicit shared_count(std::unique_ptr<Y, D>& r)
        : pi_(nullptr)
    {
        typedef typename sp_convert_reference<D>::type D2;

        D2 d2(r.get_deleter());
        pi_ = new sp_counted_impl_pd<typename std::unique_ptr<Y, D>::pointer, D2>(r.get(), d2);

        r.release();
    }

    ~shared_count() // nothrow
    {
        if (pi_ != nullptr)
            pi_->release();
    }

    shared_count(shared_count const& r)
          : pi_(r.pi_) // nothrow
    {
        if (pi_ != nullptr)
            pi_->add_ref_copy();
    }

    shared_count(shared_count&& r)
        : pi_(r.pi_) // nothrow
    {
        r.pi_ = nullptr;
    }

    explicit shared_count(weak_count const& r); // throws bad_local_weak_ptr when r.use_count() == 0
    shared_count(weak_count const& r,
                 sp_nothrow_tag); // constructs an empty *this when r.use_count() == 0

    shared_count& operator=(shared_count const& r) // nothrow
    {
        sp_counted_base* tmp = r.pi_;

        if (tmp != pi_)
        {
            if (tmp != nullptr)
                tmp->add_ref_copy();
            if (pi_ != nullptr)
                pi_->release();
            pi_ = tmp;
        }

        return *this;
    }

    void swap(shared_count& r) // nothrow
    {
        sp_counted_base *tmp = r.pi_;
        r.pi_ = pi_;
        pi_ = tmp;
    }

    long use_count() const // nothrow
    {
        return pi_ != nullptr ? pi_->use_count() : 0;
    }

    bool unique() const // nothrow
    {
        return use_count() == 1;
    }

    bool empty() const // nothrow
    {
        return pi_ == nullptr;
    }

    friend inline bool operator==(shared_count const& a, shared_count const& b)
    {
        return a.pi_ == b.pi_;
    }

    friend inline bool operator<(shared_count const& a, shared_count const& b)
    {
        return std::less<sp_counted_base *>()(a.pi_, b.pi_);
    }

    void* get_deleter(std::type_info const& ti) const
    {
        return pi_ ? pi_->get_deleter(ti) : nullptr;
    }

    void* get_untyped_deleter() const
    {
        return pi_ ? pi_->get_untyped_deleter() : nullptr;
    }
};

class weak_count
{
private:
    sp_counted_base* pi_;

    friend class shared_count;

public:
    weak_count()
        : pi_(nullptr) // nothrow
    {
        // Nothing
    }

    weak_count(shared_count const& r)
        : pi_(r.pi_) // nothrow
    {
        if (pi_ != nullptr)
            pi_->weak_add_ref();
    }

    weak_count(weak_count const& r)
        : pi_(r.pi_) // nothrow
    {
        if (pi_ != nullptr)
            pi_->weak_add_ref();
    }

    // Move support

    weak_count(weak_count&& r)
        : pi_(r.pi_) // nothrow
    {
        r.pi_ = nullptr;
    }

    ~weak_count() // nothrow
    {
        if (pi_ != nullptr)
            pi_->weak_release();
    }

    weak_count &operator=(shared_count const& r) // nothrow
    {
        sp_counted_base* tmp = r.pi_;

        if (tmp != pi_)
        {
            if (tmp != nullptr)
                tmp->weak_add_ref();
            if (pi_ != nullptr)
                pi_->weak_release();
            pi_ = tmp;
        }

        return *this;
    }

    weak_count &operator=(weak_count const& r) // nothrow
    {
        sp_counted_base* tmp = r.pi_;

        if (tmp != pi_)
        {
            if (tmp != nullptr)
                tmp->weak_add_ref();
            if (pi_ != nullptr)
                pi_->weak_release();
            pi_ = tmp;
        }

        return *this;
    }

    void swap(weak_count& r) // nothrow
    {
        sp_counted_base* tmp = r.pi_;
        r.pi_ = pi_;
        pi_ = tmp;
    }

    long use_count() const // nothrow
    {
        return pi_ != nullptr ? pi_->use_count() : 0;
    }

    bool empty() const // nothrow
    {
        return pi_ == nullptr;
    }

    friend inline bool operator==(weak_count const& a, weak_count const& b)
    {
        return a.pi_ == b.pi_;
    }

    friend inline bool operator<(weak_count const& a, weak_count const& b)
    {
        return std::less<sp_counted_base*>()(a.pi_, b.pi_);
    }
};

inline shared_count::shared_count(weak_count const& r)
    : pi_(r.pi_)
{
      if (pi_ == nullptr || !pi_->add_ref_lock())
        throw base::bad_local_weak_ptr();
}

inline shared_count::shared_count(weak_count const& r, sp_nothrow_tag)
    : pi_(r.pi_)
{
    if (pi_ != nullptr && !pi_->add_ref_lock())
        pi_ = nullptr;
}

} // namespace base::detail

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_SHARED_COUNT_H
