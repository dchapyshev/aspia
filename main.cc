//
// PROJECT:         Aspia Remote Desktop
// FILE:            main.cc
// LICENSE:         See top-level directory
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

using namespace aspia;

int main(int argc, char *argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    FLAGS_log_dir = "c:\\temp";

    if (FLAGS_run_mode.empty())
    {
        if (IsWindowsVistaOrGreater() && !IsProcessElevated())
        {
            if (!ElevateProcess())
                RunUIMain();
        }
        else
        {
            RunUIMain();
        }
    }
    else
    {
        std::wstring run_mode;
        CHECK(ANSItoUNICODE(FLAGS_run_mode, run_mode));
        RunHostMain(run_mode);
    }

    google::ShutdownGoogleLogging();
    gflags::ShutDownCommandLineFlags();

    return 0;
}
