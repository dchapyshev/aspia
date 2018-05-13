//
// PROJECT:         Aspia
// FILE:            base/win/security_helpers.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SECURITY_HELPERS_H
#define _ASPIA_BASE__WIN__SECURITY_HELPERS_H

namespace aspia {

// Initializes COM security of the process applying the passed security
// descriptor.  The function configures the following settings:
//  - Server authenticates that all data received is from the expected client.
//  - Server can impersonate clients to check their identity but cannot act on
//    their behalf.
//  - Caller's identity is verified on every call (Dynamic cloaking).
//  - Unless |activate_as_activator| is true, activations where the server
//    would run under this process's identity are prohibited.
bool initializeComSecurity(const wchar_t* security_descriptor,
                           const wchar_t* mandatory_label,
                           bool activate_as_activator);

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SECURITY_HELPERS_H
