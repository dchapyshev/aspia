//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/security.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SECURITY_H
#define _ASPIA_BASE__SECURITY_H

#include "aspia_config.h"

#include <string>

#include "base/scoped_local.h"

namespace aspia {

bool GetUserSidString(std::wstring* user_sid);

ScopedSD ConvertSddlToSd(const std::wstring& sddl);

} // namespace aspia

#endif // _ASPIA_BASE__SECURITY_H
