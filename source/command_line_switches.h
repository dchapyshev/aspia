//
// PROJECT:         Aspia
// FILE:            command_line_switches.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_COMMAND_LINE_SWITCHES_H
#define _ASPIA_COMMAND_LINE_SWITCHES_H

#include "base/command_line.h"

namespace aspia {

extern const CommandLine::CharType kModeSwitch[];
extern const CommandLine::CharType kSessionIdSwitch[];
extern const CommandLine::CharType kChannelIdSwitch[];
extern const CommandLine::CharType kServiceIdSwitch[];
extern const CommandLine::CharType kLauncherModeSwitch[];

extern const CommandLine::CharType kModeSessionLauncher[];
extern const CommandLine::CharType kModeDesktopSession[];
extern const CommandLine::CharType kModeFileTransferSession[];
extern const CommandLine::CharType kModeSystemInfoSession[];
extern const CommandLine::CharType kModeSystemInfo[];
extern const CommandLine::CharType kModeHostService[];

extern const CommandLine::CharType kInstallHostServiceSwitch[];
extern const CommandLine::CharType kRemoveHostServiceSwitch[];

extern const CommandLine::CharType kAddressBookSwitch[];

} // namespace aspia

#endif // _ASPIA_COMMAND_LINE_SWITCHES_H
