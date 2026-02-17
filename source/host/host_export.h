//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_HOST_EXPORT_H
#define HOST_HOST_EXPORT_H

#if defined(_WIN32)

#if defined(HOST_IMPLEMENTATION)
#define HOST_EXPORT __declspec(dllexport)
#else
#define HOST_EXPORT __declspec(dllimport)
#endif // defined(HOST_IMPLEMENTATION)

#else

#if defined(HOST_IMPLEMENTATION)
#define HOST_EXPORT __attribute__((visibility("default")))
#else
#define HOST_EXPORT
#endif

#endif

#endif // HOST_HOST_EXPORT_H
