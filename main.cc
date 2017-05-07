//
// PROJECT:         Aspia Remote Desktop
// FILE:            main.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>

#pragma warning(push)
#pragma warning(disable:4091)
#include <dbghelp.h>
#pragma warning(pop)

#include <gflags/gflags.h>
#include "base/message_loop/message_loop.h"
#include "base/scoped_com_initializer.h"
#include "base/service_manager.h"
#include "base/elevation_helpers.h"
#include "base/process.h"
#include "base/path.h"
#include "base/unicode.h"
#include "host/desktop_session_launcher.h"
#include "host/host_service.h"
#include "host/sas_injector.h"
#include "host/desktop_session_client.h"
#include "network/scoped_wsa_initializer.h"
#include "ui/main_dialog.h"

namespace aspia {

DEFINE_string(run_mode, "", "Run Mode");
DEFINE_uint32(session_id, 0xFFFFFFFF, "Session Id");
DEFINE_string(input_channel_id, "", "Input channel Id");
DEFINE_string(output_channel_id, "", "Output channel Id");
DEFINE_string(service_id, "", "Service Id");

static void StartGUI()
{
    ScopedCOMInitializer com_initializer;
    ScopedWsaInitializer wsa_initializer;

    CHECK(com_initializer.IsSucceeded());
    CHECK(wsa_initializer.IsSucceeded());

    INITCOMMONCONTROLSEX iccx;

    iccx.dwSize = sizeof(iccx);
    iccx.dwICC = ICC_BAR_CLASSES;

    InitCommonControlsEx(&iccx);

    MainDialog main_dialog;
    main_dialog.DoModal(nullptr);
}

} // namespace aspia

using namespace aspia;

LONG WINAPI CustomUnhandledExceptionFilter(PEXCEPTION_POINTERS info)
{
    WCHAR dump_dir_path[MAX_PATH - 14];

    if (!GetCurrentDirectoryW(_countof(dump_dir_path), dump_dir_path))
    {
        HRESULT hr = StringCchCopyW(dump_dir_path, _countof(dump_dir_path), L"c:");
        if (FAILED(hr))
            return EXCEPTION_EXECUTE_HANDLER;
    }

    GUID guid;

    if (CoCreateGuid(&guid) != S_OK)
        return EXCEPTION_EXECUTE_HANDLER;

    WCHAR uuid_string[64];

    if (!StringFromGUID2(guid, uuid_string, _countof(uuid_string)))
        return EXCEPTION_EXECUTE_HANDLER;

    WCHAR dump_file_path[MAX_PATH];

    if (FAILED(StringCchPrintfW(dump_file_path,
                                _countof(dump_file_path),
                                L"%s\\%u.%s.dmp",
                                dump_dir_path,
                                GetCurrentProcessId(),
                                uuid_string)))
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    ScopedHandle dump_file(CreateFileW(dump_file_path,
                                       GENERIC_WRITE,
                                       0,
                                       nullptr,
                                       CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL,
                                       nullptr));

    if (!dump_file.IsValid())
        return EXCEPTION_EXECUTE_HANDLER;

    MINIDUMP_EXCEPTION_INFORMATION exception;
    exception.ThreadId          = GetCurrentThreadId();
    exception.ExceptionPointers = info;
    exception.ClientPointers    = FALSE;

    MiniDumpWriteDump(GetCurrentProcess(),
                      GetCurrentProcessId(),
                      dump_file,
                      MiniDumpNormal,
                      &exception,
                      nullptr,
                      nullptr);

    LOG(ERROR) << "Memory dump written to: " << UTF8fromUNICODE(dump_file_path);
    google::FlushLogFiles(0);

    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    SetUnhandledExceptionFilter(CustomUnhandledExceptionFilter);

    std::wstring run_mode          = UNICODEfromUTF8(FLAGS_run_mode);
    std::wstring input_channel_id  = UNICODEfromUTF8(FLAGS_input_channel_id);
    std::wstring output_channel_id = UNICODEfromUTF8(FLAGS_output_channel_id);
    std::wstring service_id        = UNICODEfromUTF8(FLAGS_service_id);

    if (!Process::Current().HasAdminRights())
    {
        if (run_mode == kDesktopSessionSwitch)
        {
            DesktopSessionClient().Run(input_channel_id, output_channel_id);
        }
        else
        {
            if (!ElevateProcess())
                StartGUI();
        }
    }
    else
    {
        if (run_mode == kSasServiceSwitch)
        {
            SasInjector injector(service_id);
            injector.ExecuteService();
        }
        else if (run_mode == kHostServiceSwitch)
        {
            HostService().Run();
        }
        else if (run_mode == kInstallHostServiceSwitch)
        {
            HostService::Install();
        }
        else if (run_mode == kRemoveHostServiceSwitch)
        {
            HostService::Remove();
        }
        else if (run_mode == kDesktopSessionLauncherSwitch)
        {
            DesktopSessionLauncher launcher(service_id);

            launcher.ExecuteService(FLAGS_session_id, input_channel_id, output_channel_id);
        }
        else if (run_mode == kDesktopSessionSwitch)
        {
            DesktopSessionClient().Run(input_channel_id, output_channel_id);
        }
        else
        {
            StartGUI();
        }
    }

    google::ShutdownGoogleLogging();
    gflags::ShutDownCommandLineFlags();

    return 0;
}
