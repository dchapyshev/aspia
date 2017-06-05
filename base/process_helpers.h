//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/process_helpers.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROCESS_HELPERS_H
#define _ASPIA_BASE__PROCESS_HELPERS_H

namespace aspia {

bool ElevateProcess();
bool IsProcessElevated();

bool IsCallerAdminGroupMember();
bool IsCallerHasAdminRights();

bool IsRunningAsService();

} // namespace aspia

#endif // _ASPIA_BASE__PROCESS_HELPERS_H
