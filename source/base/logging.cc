//
// PROJECT:         Aspia
// FILE:            base/logging.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"

namespace aspia {

SystemErrorCode GetLastSystemErrorCode()
{
    return GetLastError();
}

std::string SystemErrorCodeToString(SystemErrorCode error_code)
{
    const size_t kErrorMessageBufferSize = 256;
    char buffer[kErrorMessageBufferSize];

    const DWORD flags =
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    DWORD len = FormatMessageA(flags,
                               nullptr,
                               error_code,
                               0,
                               buffer,
                               _countof(buffer),
                               nullptr);
    if (len)
    {
        // Messages returned by system end with line breaks.
        return CollapseWhitespaceASCII(buffer, true) +
            StringPrintf(" (0x%X)", error_code);
    }

    return StringPrintf("Error (0x%X) while retrieving error. (0x%X)",
                        GetLastError(), error_code);
}

std::string GetLastSystemErrorString()
{
    return SystemErrorCodeToString(GetLastSystemErrorCode());
}

std::ostream& operator<<(std::ostream& out, const wchar_t* str)
{
    out << ANSIfromUNICODE(str);
    return out;
}

std::ostream& operator<<(std::ostream& out, const std::wstring& str)
{
    out << ANSIfromUNICODE(str);
    return out;
}

} // namespace aspia
