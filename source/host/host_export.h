//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA__HOST_EXPORT_H_
#define ASPIA__HOST_EXPORT_H_

#if defined(HOST_IMPLEMENTATION)
#define HOST_EXPORT __declspec(dllexport)
#else
#define HOST_EXPORT __declspec(dllimport)
#endif // defined(HOST_IMPLEMENTATION)

#endif // ASPIA__HOST_EXPORT_H_
