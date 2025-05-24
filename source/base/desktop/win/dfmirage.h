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

#ifndef BASE_DESKTOP_WIN_DFMIRAGE_H
#define BASE_DESKTOP_WIN_DFMIRAGE_H

#include <QtGlobal>

#include "build/build_config.h"

namespace base {

enum
{
    DMF_ESCAPE_BASE_1_VB = 1030,
    DMF_ESCAPE_BASE_2_VB = 1026,
    DMF_ESCAPE_BASE_3_VB = 24
};

#if (ARCH_CPU_64_BITS == 1)

#define CLIENT_64BIT   0x8000

enum
{
    DMF_ESCAPE_BASE_1 = CLIENT_64BIT | DMF_ESCAPE_BASE_1_VB,
    DMF_ESCAPE_BASE_2 = CLIENT_64BIT | DMF_ESCAPE_BASE_2_VB,
    DMF_ESCAPE_BASE_3 = CLIENT_64BIT | DMF_ESCAPE_BASE_3_VB,
};

#else

enum
{
    DMF_ESCAPE_BASE_1 = DMF_ESCAPE_BASE_1_VB,
    DMF_ESCAPE_BASE_2 = DMF_ESCAPE_BASE_2_VB,
    DMF_ESCAPE_BASE_3 = DMF_ESCAPE_BASE_3_VB,
};

#endif // ARCH_CPU_*_BITS

typedef enum
{
    // Create the R3 mapping of update-stream pipe.
    DFM_ESC_USM_PIPE_MAP = DMF_ESCAPE_BASE_1 + 0,

    // Release the mapping.
    DFM_ESC_USM_PIPE_UNMAP = DMF_ESCAPE_BASE_1 + 1,
} DfmEscape;

static const int kDfmMaxChanges = 20000;

typedef struct
{
    long left;
    long top;
    long right;
    long bottom;
} DfmRect;

typedef struct
{
    long x;
    long y;
} DfmPoint;

typedef struct
{
    quint32 type;
    DfmRect rect;
    DfmRect origrect;
    DfmPoint point;
    quint32 color;
    quint32 refcolor;
} DfmChangesRecord;

typedef struct
{
    quint32 counter;
    DfmChangesRecord records[kDfmMaxChanges];
} DfmChangesBuffer;

typedef struct
{
    DfmChangesBuffer* changes_buffer;
    quint8* user_buffer;
} DfmGetChangesBuffer;

} // base

#endif // BASE_DESKTOP_WIN_DFMIRAGE_H
