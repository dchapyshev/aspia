//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_clipboard.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_CLIPBOARD_H
#define _ASPIA_BASE__SCOPED_CLIPBOARD_H

#include "base/macros.h"

namespace aspia {

class ScopedClipboard
{
public:
    ScopedClipboard() = default;
    ~ScopedClipboard();

    bool Init(HWND owner);
    BOOL Empty();
    void SetData(UINT format, HANDLE mem);

    //
    // The caller must not free the handle. The caller should lock the handle,
    // copy the clipboard data, and unlock the handle. All this must be done
    // before this ScopedClipboard is destroyed.
    //
    HANDLE GetData(UINT format) const;

private:
    bool opened_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedClipboard);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_CLIPBOARD_H
