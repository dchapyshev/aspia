//
// PROJECT:         Aspia
// FILE:            base/process/process_helpers.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROCESS__PROCESS_HELPERS_H
#define _ASPIA_BASE__PROCESS__PROCESS_HELPERS_H

#include "base/command_line.h"

namespace aspia {

bool IsProcessElevated();

bool LaunchProcessWithElevate(const std::experimental::filesystem::path& program_path);
bool LaunchProcessWithElevate(const CommandLine& command_line);

bool LaunchProcess(const std::experimental::filesystem::path& program_path);
bool LaunchProcess(const CommandLine& command_line);

bool IsCallerAdminGroupMember();
bool IsCallerHasAdminRights();

} // namespace aspia

#endif // _ASPIA_BASE__PROCESS__PROCESS_HELPERS_H
