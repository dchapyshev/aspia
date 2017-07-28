//
// PROJECT:         Aspia Remote Desktop
// FILE:            main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include <gflags/gflags.h>
#include "base/version_helpers.h"
#include "base/process/process_helpers.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
#include "host/host_main.h"
#include "ui/ui_main.h"

DEFINE_string(run_mode, "", "Run Mode");

int main(int argc, char *argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    FLAGS_log_dir = "c:\\temp";

    if (FLAGS_run_mode.empty())
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
        std::wstring run_mode;
        CHECK(aspia::ANSItoUNICODE(FLAGS_run_mode, run_mode));
        aspia::RunHostMain(run_mode);
    }

    google::ShutdownGoogleLogging();
    gflags::ShutDownCommandLineFlags();

    return 0;
}
