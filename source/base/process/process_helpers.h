//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/process/process_helpers.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROCESS__PROCESS_HELPERS_H
#define _ASPIA_BASE__PROCESS__PROCESS_HELPERS_H

namespace aspia {

bool ElevateProcess();
bool IsProcessElevated();

bool IsCallerAdminGroupMember();
bool IsCallerHasAdminRights();

bool IsRunningAsService();

} // namespace aspia

#endif // _ASPIA_BASE__PROCESS__PROCESS_HELPERS_H
