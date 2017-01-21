//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/clipboard.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__CLIPBOARD_H
#define _ASPIA_BASE__CLIPBOARD_H

#include "aspia_config.h"

#include <string>

namespace aspia {

class Clipboard
{
public:
    static void Set(const std::string &str);
    static std::string Get();
};

} // namespace aspia

#endif // _ASPIA_BASE__CLIPBOARD_H
