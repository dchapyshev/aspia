//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/clipboard.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/clipboard.h"

#include <strsafe.h>

#include "base/unicode.h"

namespace aspia {

// static
void Clipboard::Set(const std::string &str)
{
    std::wstring uni = UNICODEfromUTF8(str);

    if (!uni.empty())
    {
        if (OpenClipboard(nullptr))
        {
            EmptyClipboard();

            SIZE_T buffer_size = (uni.length() + 1) * sizeof(WCHAR);

            HGLOBAL hGlobal = GlobalAlloc(GHND, buffer_size);
            if (hGlobal)
            {
                WCHAR *buffer = reinterpret_cast<WCHAR*>(GlobalLock(hGlobal));
                if (buffer)
                {
                    StringCbCopyW(buffer, buffer_size, uni.c_str());
                    SetClipboardData(CF_UNICODETEXT, buffer);
                    GlobalUnlock(buffer);
                }
                GlobalFree(hGlobal);
            }

            CloseClipboard();
        }
    }
}

// static
std::string Clipboard::Get()
{
    if (OpenClipboard(nullptr))
    {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData)
        {
            WCHAR *uni = reinterpret_cast<WCHAR*>(GlobalLock(hData));
            if (uni)
            {
                std::string str = UTF8fromUNICODE(uni);
                GlobalUnlock(hData);
                return str;
            }
        }

        CloseClipboard();
    }

    return std::string();
}

} // namespace aspia
