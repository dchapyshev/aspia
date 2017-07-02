//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/imagelist.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__IMAGELIST_H
#define _ASPIA_UI__BASE__IMAGELIST_H

#include "base/macros.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class CImageListCustom : public CImageListManaged
{
public:
    CImageListCustom() = default;
    ~CImageListCustom() = default;

    BOOL CreateSmall()
    {
        return Create(GetSystemMetrics(SM_CXSMICON),
                      GetSystemMetrics(SM_CYSMICON),
                      ILC_MASK | GetICLColor(),
                      1, 1);
    }

private:
    static int GetICLColor()
    {
        DEVMODEW mode = { 0 };
        mode.dmSize = sizeof(mode);

        if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode))
        {
            switch (mode.dmBitsPerPel)
            {
                case 32:
                    return ILC_COLOR32;

                case 24:
                    return ILC_COLOR24;

                case 16:
                    return ILC_COLOR16;

                case 8:
                    return ILC_COLOR8;

                case 4:
                    return ILC_COLOR4;
            }
        }

        return ILC_COLOR32;
    }

    DISALLOW_COPY_AND_ASSIGN(CImageListCustom);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__IMAGELIST_H
