//
// PROJECT:         Aspia
// FILE:            command_line_switches.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "command_line_switches.h"

namespace aspia {

const CommandLine::CharType kModeSwitch[] = L"mode";
const CommandLine::CharType kSessionIdSwitch[] = L"session-id";
const CommandLine::CharType kChannelIdSwitch[] = L"channel-id";
const CommandLine::CharType kServiceIdSwitch[] = L"service-id";
const CommandLine::CharType kLauncherModeSwitch[] = L"launcher-mode";

const CommandLine::CharType kModeSessionLauncher[] = L"session-launcher";
const CommandLine::CharType kModeDesktopSession[] = L"desktop-session";
const CommandLine::CharType kModeFileTransferSession[] = L"file-transfer-session";
const CommandLine::CharType kModePowerManageSession[] = L"power-manage-session";
const CommandLine::CharType kModeSystemInfoSession[] = L"system-info-session";
const CommandLine::CharType kModeSystemInfo[] = L"system-info";
const CommandLine::CharType kModeHostService[] = L"host-service";

const CommandLine::CharType kInstallHostServiceSwitch[] = L"install-host-service";
const CommandLine::CharType kRemoveHostServiceSwitch[] = L"remove-host-service";

const CommandLine::CharType kAddressBookSwitch[] = L"address-book";

} // namespace aspia
