//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/security.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/security.h"
#include "base/scoped_handle.h"
#include "base/unicode.h"
#include "base/util.h"

#include <sddl.h>
#include <memory>

namespace aspia {

bool GetUserSidString(std::wstring* user_sid)
{
    // Get the current token.
    ScopedHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.Recieve()))
        return false;

    DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
    std::unique_ptr<BYTE[]> user_bytes(new BYTE[size]);

    TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

    if (!GetTokenInformation(token, TokenUser, user, size, &size))
        return false;

    if (!user->User.Sid)
        return false;

    // Convert the data to a string.
    ScopedLocal<WCHAR*> sid_string;

    if (!ConvertSidToStringSidW(user->User.Sid, sid_string.Recieve()))
        return false;

    *user_sid = sid_string;

    return true;
}

ScopedSD ConvertSddlToSd(const std::wstring& sddl)
{
    ScopedSD sd;
    ULONG length = 0;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),
                                                              SDDL_REVISION_1,
                                                              sd.Recieve(),
                                                              &length))
    {
        LOG(ERROR) << "ConvertStringSecurityDescriptorToSecurityDescriptorW() failed: "
            << GetLastError();
        return ScopedSD();
    }

    return sd;
}

} // namespace aspia
