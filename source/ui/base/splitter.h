//
// PROJECT:         Aspia
// FILE:            ui/base/splitter.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__SPLITTER_H
#define _ASPIA_UI__BASE__SPLITTER_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlsplit.h>
#include <atlmisc.h>

namespace aspia {

template <bool kIsVertical>
class SplitterT :
    public CSplitterWindowImpl<SplitterT<kIsVertical>>
{
public:
    DECLARE_WND_CLASS_EX(L"UiSplitter", CS_DBLCLKS, COLOR_WINDOW)

    SplitterT() : CSplitterWindowImpl<SplitterT<kIsVertical>>(kIsVertical)
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

using VerticalSplitter = SplitterT<true>;
using HorizontalSplitter = SplitterT<false>;

} // namespace aspia

#endif // _ASPIA_UI__BASE__SPLITTER_H
