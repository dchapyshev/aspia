//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/splitter.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__SPLITTER_H
#define _ASPIA_UI__BASE__SPLITTER_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlsplit.h>

namespace aspia {

template <bool is_vertical>
class UiSplitterT :
    public CSplitterWindowImpl<UiSplitterT<is_vertical>>
{
public:
    DECLARE_WND_CLASS_EX(L"UiSplitter", CS_DBLCLKS, COLOR_WINDOW)

    UiSplitterT() : CSplitterWindowImpl<UiSplitterT<is_vertical>>(is_vertical)
    {
        // Nothing
    }

    void DrawSplitterBar(CDCHandle dc)
    {
        CRect rect;

        if (GetSplitterBarRect(rect))
        {
            dc.FillRect(rect, COLOR_WINDOW);
        }
    }
};

using UiVerticalSplitter = UiSplitterT<true>;
using UiHorizontalSplitter = UiSplitterT<false>;

} // namespace aspia

#endif // _ASPIA_UI__BASE__SPLITTER_H
