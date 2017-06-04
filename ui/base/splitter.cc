//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/splitter.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/splitter.h"

namespace aspia {

void Splitter::Create(HWND parent, int position, int min_first, int min_second)
{
    position_ = position;
    min_first_ = min_first;
    min_second_ = min_second;
}

void Splitter::SetPanels(HWND first, HWND second)
{
    first_ = first;
    second_ = second;
}

bool Splitter::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)
{
    switch (msg)
    {
        case WM_SIZE:
        {

        }
        break;

        default:
            return false;
    }

    *result = 0;
    return true;
}

} // namespace aspia
