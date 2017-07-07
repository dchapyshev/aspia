//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/splitter.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__SPLITTER_H
#define _ASPIA_UI__BASE__SPLITTER_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlsplit.h>

namespace aspia {

template <bool kIsVertical>
class UiSplitterT :
    public CSplitterWindowImpl<UiSplitterT<kIsVertical>>
{
public:
    DECLARE_WND_CLASS_EX(L"UiSplitter", CS_DBLCLKS, COLOR_WINDOW)

    UiSplitterT() : CSplitterWindowImpl<UiSplitterT<kIsVertical>>(kIsVertical)
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
