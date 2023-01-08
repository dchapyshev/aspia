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
// Copyright (c) 2003 Daniel Frey
// Copyright (c) 2003 Howard Hinnant
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_CHECKED_DELETE_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_CHECKED_DELETE_H

namespace base {

// verify that types are complete for increased safety

template <class T>
inline void checked_delete(T* x)
{
    // intentionally complex - simplification causes regressions
    typedef char type_must_be_complete[sizeof(T) ? 1 : -1];
    (void)sizeof(type_must_be_complete);
    delete x;
}

template <class T>
inline void checked_array_delete(T* x)
{
    typedef char type_must_be_complete[sizeof(T) ? 1 : -1];
    (void)sizeof(type_must_be_complete);
    delete[] x;
}

template <class T>
struct checked_deleter
{
    typedef void result_type;
    typedef T* argument_type;

    void operator()(T* x) const
    {
        base::checked_delete(x);
    }
};

template <class T>
struct checked_array_deleter
{
    typedef void result_type;
    typedef T *argument_type;

    void operator()(T *x) const
    {
        base::checked_array_delete(x);
    }
};

} // namespace base

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_CHECKED_DELETE_H
