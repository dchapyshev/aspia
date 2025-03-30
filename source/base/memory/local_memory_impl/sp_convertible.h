//
// Aspia Project
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
// This file forked from Boost.
// Copyright 2008 Peter Dimov
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_CONVERTIBLE_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_CONVERTIBLE_H

#include <cstddef>

namespace base::detail {

template <class Y, class T>
struct sp_convertible
{
    typedef char (&yes)[1];
    typedef char (&no)[2];

    static yes f(T*);
    static no f(...);

    enum _vt { value = sizeof((f)(static_cast<Y *>(nullptr))) == sizeof(yes) };
};

template <class Y, class T>
struct sp_convertible<Y, T[]>
{
    enum _vt { value = false };
};

template <class Y, class T>
struct sp_convertible<Y[], T[]>
{
    enum _vt { value = sp_convertible<Y[1], T[1]>::value };
};

template <class Y, std::size_t N, class T>
struct sp_convertible<Y[N], T[]>
{
    enum _vt { value = sp_convertible<Y[1], T[1]>::value };
};

struct sp_empty {};

template <bool>
struct sp_enable_if_convertible_impl;

template <>
struct sp_enable_if_convertible_impl<true>
{
    typedef sp_empty type;
};

template <>
struct sp_enable_if_convertible_impl<false> {};

template <class Y, class T>
struct sp_enable_if_convertible : public sp_enable_if_convertible_impl<sp_convertible<Y, T>::value> {};

} // namespace base::detail

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_SP_CONVERTIBLE_H
