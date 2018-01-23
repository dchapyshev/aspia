//
// PROJECT:         Aspia
// FILE:            command_line_switches.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "command_line_switches.h"

namespace aspia {

const CommandLine::CharType kRunModeSwitch[] = L"run_mode";
const CommandLine::CharType kSessionIdSwitch[] = L"session_id";
const CommandLine::CharType kChannelIdSwitch[] = L"channel_id";
const CommandLine::CharType kServiceIdSwitch[] = L"service_id";
const CommandLine::CharType kLauncherModeSwitch[] = L"launcher_mode";

const CommandLine::CharType kRunModeSessionLauncher[] = L"session-launcher";
const CommandLine::CharType kRunModeDesktopSession[] = L"desktop-session";
const CommandLine::CharType kRunModeFileTransferSession[] = L"file-transfer-session";
const CommandLine::CharType kRunModePowerManageSession[] = L"power-manage-session";
const CommandLine::CharType kRunModeSystemInfoSession[] = L"system-info-session";
const CommandLine::CharType kRunModeSystemInfo[] = L"system-info";
const CommandLine::CharType kRunModeHostService[] = L"host-service";

const CommandLine::CharType kInstallHostServiceSwitch[] = L"install-host-service";
const CommandLine::CharType kRemoveHostServiceSwitch[] = L"remove-host-service";

const CommandLine::CharType kAddressBookSwitch[] = L"address-book";

} // namespace aspia
