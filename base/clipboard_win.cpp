/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/clipboard_win.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/clipboard_win.h"

// static
void Clipboard::Set(const std::string &str)
{
    std::wstring uni = UNICODEfromUTF8(str);

    if (!uni.empty())
    {
        if (OpenClipboard(nullptr))
        {
            EmptyClipboard();

            SIZE_T buffer_size = (uni.length() + 1) * sizeof(wchar_t);

            HGLOBAL hGlobal = GlobalAlloc(GHND, buffer_size);
            if (hGlobal)
            {
                wchar_t *buffer = reinterpret_cast<wchar_t*>(GlobalLock(hGlobal));
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
            wchar_t *uni = reinterpret_cast<wchar_t*>(GlobalLock(hData));
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
