//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
// Copyright (c) 2002, 2003 Peter Dimov
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_ENABLE_SHARED_FROM_THIS_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_ENABLE_SHARED_FROM_THIS_H

#include "base/memory/local_memory_impl/local_shared_ptr.h"
#include "base/memory/local_memory_impl/local_weak_ptr.h"

#include <cassert>

namespace base {

template <class T>
class enable_shared_from_this
{
protected:
    enable_shared_from_this() noexcept = default;
    enable_shared_from_this(enable_shared_from_this const&) noexcept = default;

    enable_shared_from_this &operator=(enable_shared_from_this const&) noexcept
    {
        return *this;
    }

    // ~weak_ptr<T> newer throws, so this call also must not throw
    ~enable_shared_from_this() noexcept = default;

public:
    local_shared_ptr<T> shared_from_this()
    {
        local_shared_ptr<T> p(weak_this_);
        assert(p.get() == this);
        return p;
    }

    local_shared_ptr<T const> shared_from_this() const
    {
        local_shared_ptr<T const> p(weak_this_);
        assert(p.get() == this);
        return p;
    }

    local_weak_ptr<T> weak_from_this() noexcept { return weak_this_; }
    local_weak_ptr<T const> weak_from_this() const noexcept { return weak_this_; }

public: // actually private, but avoids compiler template friendship issues
    // Note: invoked automatically by local_shared_ptr; do not call
    template <class X, class Y>
    void _internal_accept_owner(local_shared_ptr<X> const* ppx, Y *py) const
    {
        if (weak_this_.expired())
            weak_this_ = local_shared_ptr<T>(*ppx, py);
    }

private:
    mutable local_weak_ptr<T> weak_this_;
};

} // namespace base

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_ENABLE_SHARED_FROM_THIS_H
