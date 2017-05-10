//
// PROJECT:         Aspia Remote Desktop
// FILE:            main.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include <gflags/gflags.h>
#include "base/elevation_helpers.h"
#include "base/process.h"
#include "base/unicode.h"
#include "base/logging.h"
#include "host/host_main.h"
#include "ui/ui_main.h"

DEFINE_string(run_mode, "", "Run Mode");

using namespace aspia;

int main(int argc, char *argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    if (FLAGS_run_mode.empty())
    {
        if (Process::Current().HasAdminRights())
        {
            RunUIMain();
        }
        else
        {
            if (!ElevateProcess())
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
