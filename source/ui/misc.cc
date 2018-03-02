//
// PROJECT:         Aspia
// FILE:            ui/misc.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/misc.h"

#include <atlmisc.h>
#include <shellapi.h>

#include "ui/resource.h"

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
