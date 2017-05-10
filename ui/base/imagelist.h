//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/imagelist.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__IMAGELIST_H
#define _ASPIA_UI__BASE__IMAGELIST_H

#include "base/macros.h"
#include "base/logging.h"
#include "base/scoped_user_object.h"
#include "ui/base/module.h"

#include <commctrl.h>

namespace aspia {

class ImageList
{
public:
    ImageList() = default;

    ~ImageList()
    {
        Close();
    }

    bool Create(int width, int height, UINT flags, int initial, int grow)
    {
        Close();

        list_ = ImageList_Create(width, height, flags, initial, grow);
        if (!list_)
            return false;

        return true;
    }

    int AddIcon(HICON icon)
    {
        return ImageList_AddIcon(list_, icon);
    }

    int AddIcon(const Module& module, UINT resource_id, int width, int height)
    {
        ScopedHICON icon(module.icon(resource_id, width, height, LR_CREATEDIBSECTION));
        return AddIcon(icon);
    }

    HIMAGELIST Handle()
    {
        return list_;
    }

    operator HIMAGELIST()
    {
        return Handle();
    }

private:
    void Close()
    {
        if (list_)
            ImageList_Destroy(list_);
    }

    HIMAGELIST list_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ImageList);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__IMAGELIST_H
