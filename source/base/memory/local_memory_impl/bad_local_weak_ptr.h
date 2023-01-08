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
// Copyright (c) 2001, 2002, 2003 Peter Dimov and Multi Media Ltd.
//

#ifndef BASE_MEMORY_LOCAL_MEMORY_IMPL_BAD_LOCAL_WEAK_PTR_H
#define BASE_MEMORY_LOCAL_MEMORY_IMPL_BAD_LOCAL_WEAK_PTR_H

#include <exception>

namespace base {

class bad_local_weak_ptr : public std::exception
{
public:
    virtual char const *what() const throw()
    {
        return "base::bad_local_weak_ptr";
    }
};

} // namespace base

#endif // BASE_MEMORY_LOCAL_MEMORY_IMPL_BAD_LOCAL_WEAK_PTR_H
