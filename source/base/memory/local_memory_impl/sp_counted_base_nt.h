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

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_COUNTED_BASE_NT_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_COUNTED_BASE_NT_H

#include <typeinfo>

namespace base::detail {

class sp_counted_base
{
private:
    sp_counted_base(sp_counted_base const&);
    sp_counted_base& operator=(sp_counted_base const&);

    long use_count_; // #shared
    long weak_count_; // #weak + (#shared != 0)

public:
    sp_counted_base()
        : use_count_(1), weak_count_(1)
    {
        // Nothing
    }

    virtual ~sp_counted_base() = default;

    // dispose() is called when use_count_ drops to zero, to release
    // the resources managed by *this.

    virtual void dispose() = 0;

    // destroy() is called when weak_count_ drops to zero.

    virtual void destroy()
    {
        delete this;
    }

    virtual void* get_deleter(std::type_info const& ti) = 0;
    virtual void* get_untyped_deleter() = 0;

    void add_ref_copy()
    {
        ++use_count_;
    }

    bool add_ref_lock()
    {
        if (use_count_ == 0)
            return false;
        ++use_count_;
        return true;
    }

    void release()
    {
        if (--use_count_ == 0)
        {
            dispose();
            weak_release();
        }
    }

    void weak_add_ref()
    {
        ++weak_count_;
    }

    void weak_release()
    {
        if (--weak_count_ == 0)
            destroy();
    }

    long use_count() const
    {
        return use_count_;
    }
};

} // namespace base::detail

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_COUNTED_BASE_NT_H
