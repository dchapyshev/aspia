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
    ~ImageList();

    bool Create(int width, int height, UINT flags, int initial, int grow);
    bool CreateSmall();
    void RemoveAll();

    int AddIcon(HICON icon);
    int AddIcon(const Module& module, UINT resource_id);

    HIMAGELIST Handle() { return list_; }
    operator HIMAGELIST() { return Handle(); }

private:
    void Close();

    HIMAGELIST list_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ImageList);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__IMAGELIST_H
