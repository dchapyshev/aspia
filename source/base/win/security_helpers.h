//
// PROJECT:         Aspia
// FILE:            base/win/security_helpers.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SECURITY_HELPERS_H
#define _ASPIA_BASE__WIN__SECURITY_HELPERS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include "base/typed_buffer.h"

namespace aspia {

using ScopedAcl = TypedBuffer<ACL>;
using ScopedSd = TypedBuffer<SECURITY_DESCRIPTOR>;
using ScopedSid = TypedBuffer<SID>;

//
// Concatenates ACE type, permissions and sid given as SDDL strings into an ACE
// definition in SDDL form.
//
#define SDDL_ACE(type, permissions, sid) \
    L"(" type L";;" permissions L";;;" sid L")"

//
// Text representation of COM_RIGHTS_EXECUTE and COM_RIGHTS_EXECUTE_LOCAL
// permission bits that is used in the SDDL definition below.
//
#define SDDL_COM_EXECUTE_LOCAL L"0x3"

//
// Initializes COM security of the process applying the passed security
// descriptor.  The function configures the following settings:
//  - Server authenticates that all data received is from the expected client.
//  - Server can impersonate clients to check their identity but cannot act on
//    their behalf.
//  - Caller's identity is verified on every call (Dynamic cloaking).
//  - Unless |activate_as_activator| is true, activations where the server
//    would run under this process's identity are prohibited.
//
bool InitializeComSecurity(const std::wstring& security_descriptor,
                           const std::wstring& mandatory_label,
                           bool activate_as_activator);

bool GetUserSidString(std::wstring& user_sid);

ScopedSd ConvertSddlToSd(const std::wstring& sddl);

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SECURITY_HELPERS_H
