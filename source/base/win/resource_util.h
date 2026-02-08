//
// SmartCafe Project
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

// This file contains utility functions for accessing resources in external
// files (DLLs) or embedded in the executable itself.

#ifndef BASE_WIN_RESOURCE_UTIL_H
#define BASE_WIN_RESOURCE_UTIL_H

#include <cstddef>

#include <qt_windows.h>

namespace base {

// Function for getting a data resource of the specified |resource_type| from a dll.
// Some resources are optional, especially in unit tests, so this returns false but doesn't raise
// an error if the resource can't be loaded.
bool resourceFromModule(HMODULE module,
                        int resource_id,
                        const wchar_t* resource_type,
                        void** data,
                        size_t* length);

// Function for getting a data resource (BINDATA) from a dll. Some resources are optional,
// especially in unit tests, so this returns false but doesn't raise an error if the resource can't
// be loaded.
bool dataResourceFromModule(HMODULE module,
                            int resource_id,
                            void** data,
                            size_t* length);

} // namespace base

#endif // BASE_WIN_RESOURCE_UTIL_H
