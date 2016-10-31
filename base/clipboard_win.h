/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/clipboard_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CLIPBOARD_WIN_H
#define _ASPIA_CLIPBOARD_WIN_H

#include "aspia_config.h"

#include <string>
#include <windows.h>
#include <strsafe.h>

#include "base/unicode.h"

class Clipboard
{
public:
    static void Set(const std::string &str);
    static std::string Get();
};

#endif // _ASPIA_CLIPBOARD_WIN_H
