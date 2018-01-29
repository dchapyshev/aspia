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
#include "command_line_switches.h"

int WINAPI wWinMain(HINSTANCE /* hInstance */,
                    HINSTANCE /* hPrevInstance */,
                    PWSTR /* pCmdLine */,
                    int /* nCmdShow */)
{
    aspia::LoggingSettings settings;

    settings.logging_dest = aspia::LOG_TO_ALL;
    settings.lock_log = aspia::LOCK_LOG_FILE;

    aspia::InitLogging(settings);

    const aspia::CommandLine& command_line = aspia::CommandLine::ForCurrentProcess();

    if (command_line.IsEmpty())
    {
        if (aspia::IsWindowsVistaOrGreater() && !aspia::IsProcessElevated())
        {
            std::experimental::filesystem::path program_path;

            if (aspia::GetBasePath(aspia::BasePathKey::FILE_EXE, program_path))
            {
                if (!aspia::LaunchProcessWithElevate(program_path))
                    aspia::RunUIMain(aspia::UI::MAIN_DIALOG);
            }
        }
        else
        {
            aspia::RunUIMain(aspia::UI::MAIN_DIALOG);
        }
    }
    else if (command_line.HasSwitch(aspia::kAddressBookSwitch))
    {
        aspia::RunUIMain(aspia::UI::ADDRESS_BOOK);
    }
    else
    {
        aspia::RunHostMain(command_line);
    }

    aspia::ShutdownLogging();

    return 0;
}
