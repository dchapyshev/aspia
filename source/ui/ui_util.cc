//
// PROJECT:         Aspia
// FILE:            ui/ui_util.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/ui_util.h"

namespace aspia {

std::wstring GetWindowString(const CWindow& window)
{
    int length = window.GetWindowTextLengthW();
    if (!length)
        return std::wstring();

    std::wstring text;
    text.resize(length);

    if (window.GetWindowTextW(text.data(), length + 1) != length)
        return std::wstring();

    return text;
}

} // namespace aspia
