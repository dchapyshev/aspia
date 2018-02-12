//
// PROJECT:         Aspia
// FILE:            ui/ui_util.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/ui_util.h"

#include <atlmisc.h>
#include <shellapi.h>

#include "base/logging.h"
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

void OpenLink(LinkTypeKey key)
{
    UINT resource_id;

    switch (key)
    {
        case LinkTypeKey::SITE:
            resource_id = IDS_SITE_LINK;
            break;

        case LinkTypeKey::HELP:
            resource_id = IDS_HELP_LINK;
            break;

        case LinkTypeKey::DONATE:
            resource_id = IDS_DONATE_LINK;
            break;

        default:
            DLOG(LS_FATAL) << "Unexpected link key: " << static_cast<int>(key);
            return;
    }

    CString url;
    url.LoadStringW(resource_id);
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace aspia
