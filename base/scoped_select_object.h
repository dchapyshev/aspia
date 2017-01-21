//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_select_object.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_SELECT_OBJECT_H
#define _ASPIA_BASE__SCOPED_SELECT_OBJECT_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/logging.h"

namespace aspia {

// Helper class for deselecting object from DC.
class ScopedSelectObject
{
public:
    ScopedSelectObject(HDC hdc, HGDIOBJ object) :
        hdc_(hdc),
        oldobj_(SelectObject(hdc, object))
    {
        DCHECK(hdc_);
        DCHECK(object);
        DCHECK(oldobj_ != NULL && oldobj_ != HGDI_ERROR);
    }

    ~ScopedSelectObject()
    {
        HGDIOBJ object = SelectObject(hdc_, oldobj_);
        DCHECK((GetObjectType(oldobj_) != OBJ_REGION && object != NULL) ||
               (GetObjectType(oldobj_) == OBJ_REGION && object != HGDI_ERROR));
    }

private:
    HDC hdc_;
    HGDIOBJ oldobj_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSelectObject);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_SELECT_OBJECT_H
