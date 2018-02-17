//
// PROJECT:         Aspia
// FILE:            main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/base_paths.h"
#include "base/process/process_helpers.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "host/host_main.h"
#include "ui/ui_main.h"
#include "ui/resource.h"
#include "command_line_switches.h"

namespace aspia {

void Main()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    const CommandLine& command_line = CommandLine::ForCurrentProcess();

    if (command_line.IsEmpty())
    {
        if (IsWindowsVistaOrGreater() && !IsProcessElevated())
        {
            std::experimental::filesystem::path program_path;

            if (GetBasePath(BasePathKey::FILE_EXE, program_path))
            {
                if (!LaunchProcessWithElevate(program_path))
                    RunUIMain(UI::MAIN_DIALOG);
            }
        }
        else
        {
            RunUIMain(UI::MAIN_DIALOG);
        }
    }
    else if (command_line.HasSwitch(kAddressBookSwitch))
    {
        RunUIMain(UI::ADDRESS_BOOK);
    }
    else
    {
        RunHostMain(command_line);
    }

    ShutdownLogging();
}

} // namespace aspia

int WINAPI wWinMain(HINSTANCE /* hInstance */,
                    HINSTANCE /* hPrevInstance */,
                    PWSTR /* pCmdLine */,
                    int /* nCmdShow */)
{
    aspia::Main();
    return 0;
}
