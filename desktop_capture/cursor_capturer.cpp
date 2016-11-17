/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/cursor_capturer.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "cursor_capturer.h"

bool CursorCapturer::Initialize()
{
    hDesktopDC_ = GetDC(nullptr);
    if (!hDesktopDC_)
    {
        LOG(ERROR) << "GetDC() failed: " << GetLastError();
        return false;
    }

    hMemoryDC_ = CreateCompatibleDC(hDesktopDC_);
    if (!hMemoryDC_)
    {
        LOG(ERROR) << "CreateCompatibleDC() failed: " << GetLastError();
        ReleaseDC(nullptr, hDesktopDC_);
        return false;
    }

    return true;
}
