//
// PROJECT:         Aspia
// FILE:            main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/command_line.h"
#include "base/version_helpers.h"
#include "base/process/process_helpers.h"
#include "base/logging.h"
#include "host/host_main.h"
#include "ui/ui_main.h"

int WINAPI wWinMain(HINSTANCE /* hInstance */,
                    HINSTANCE /* hPrevInstance */,
                    PWSTR /* pCmdLine */,
                    int /* nCmdShow */)
{
    google::InitGoogleLogging("aspia.exe");
    FLAGS_log_dir = "c:\\temp";

    const aspia::CommandLine& command_line = aspia::CommandLine::ForCurrentProcess();

    if (command_line.IsEmpty())
    {
        if (aspia::IsWindowsVistaOrGreater() && !aspia::IsProcessElevated())
        {
            if (!aspia::ElevateProcess())
                aspia::RunUIMain();
        }
        else
        {
            aspia::RunUIMain();
        }
    }
    else
    {
        aspia::RunHostMain(command_line);
    }

    google::ShutdownGoogleLogging();

    return 0;
}
