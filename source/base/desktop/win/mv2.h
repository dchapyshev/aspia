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

#ifndef BASE_DESKTOP_WIN_MV2_H
#define BASE_DESKTOP_WIN_MV2_H

#include <QtGlobal>

namespace base {

static const int kMv2MaxChanges = 2000;

typedef struct
{
    long left;
    long top;
    long right;
    long bottom;
} Mv2Rect;

typedef struct
{
    long x;
    long y;
} Mv2Point;

typedef struct
{
    quint32 type;
    Mv2Rect rect;
    Mv2Point point;
} Mv2ChangesRecord;

typedef struct
{
    quint32 counter;
    Mv2ChangesRecord records[kMv2MaxChanges];
} Mv2ChangesBuffer;

} // base

#endif // BASE_DESKTOP_WIN_MV2_H
