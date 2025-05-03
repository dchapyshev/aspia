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

#ifndef BASE_WIN_SCOPED_SELECT_OBJECT_H
#define BASE_WIN_SCOPED_SELECT_OBJECT_H

#include "base/logging.h"

#include <qt_windows.h>

namespace base {

// Helper class for deselecting object from DC.
class ScopedSelectObject
{
public:
    ScopedSelectObject(HDC hdc, HGDIOBJ object)
        : hdc_(hdc),
          oldobj_(SelectObject(hdc, object))
    {
        DCHECK(hdc_);
        DCHECK(object);
        DCHECK(oldobj_ != nullptr && oldobj_ != HGDI_ERROR);
    }

    ~ScopedSelectObject()
    {
        HGDIOBJ object = SelectObject(hdc_, oldobj_);
        DCHECK((GetObjectType(oldobj_) != OBJ_REGION && object != nullptr) ||
               (GetObjectType(oldobj_) == OBJ_REGION && object != HGDI_ERROR));
    }

private:
    HDC hdc_;
    HGDIOBJ oldobj_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSelectObject);
};

} // namespace base

#endif // BASE_WIN_SCOPED_SELECT_OBJECT_H
