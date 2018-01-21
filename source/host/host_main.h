//
// PROJECT:         Aspia
// FILE:            host/host_main.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_MAIN_H
#define _ASPIA_HOST__HOST_MAIN_H

#include "base/command_line.h"

namespace aspia {

extern const CommandLine::CharType kRunModeSwitch[];
extern const CommandLine::CharType kSessionIdSwitch[];
extern const CommandLine::CharType kChannelIdSwitch[];
extern const CommandLine::CharType kServiceIdSwitch[];
extern const CommandLine::CharType kLauncherModeSwitch[];

extern const CommandLine::CharType kRunModeSessionLauncher[];
extern const CommandLine::CharType kRunModeDesktopSession[];
extern const CommandLine::CharType kRunModeFileTransferSession[];
extern const CommandLine::CharType kRunModePowerManageSession[];
extern const CommandLine::CharType kRunModeSystemInfoSession[];
extern const CommandLine::CharType kRunModeSystemInfo[];
extern const CommandLine::CharType kRunModeHostService[];

extern const CommandLine::CharType kInstallHostServiceSwitch[];
extern const CommandLine::CharType kRemoveHostServiceSwitch[];

void RunHostMain(const CommandLine& command_line);

} // namespace aspia

#endif // _ASPIA_HOST__HOST_MAIN_H
