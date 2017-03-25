//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/main.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include <atlbase.h>
#include <atlapp.h>
#include <sddl.h>
#include <dbghelp.h>
#include <strsafe.h>

#include <gflags/gflags.h>
#include "base/scoped_local.h"
#include "base/scoped_sid.h"
#include "base/service_manager.h"
#include "base/process.h"
#include "base/security.h"
#include "base/path.h"
#include "base/unicode.h"
#include "host/desktop_session_launcher.h"
#include "host/host_service.h"
#include "host/sas_injector.h"
#include "gui/main_dialog.h"
#include "host/desktop_session_client.h"

namespace aspia {

CAppModule _module;
CIcon _small_icon;
CIcon _big_icon;

DEFINE_string(run_mode, "", "Run Mode");
DEFINE_uint32(session_id, 0xFFFFFFFF, "Session Id");
DEFINE_string(input_channel_id, "", "Input channel Id");
DEFINE_string(output_channel_id, "", "Output channel Id");
DEFINE_string(service_id, "", "Service Id");

static void StartGUI()
{
    AtlInitCommonControls(ICC_BAR_CLASSES);

    HRESULT res = _module.Init(nullptr, GetModuleHandleW(nullptr));
    CHECK(SUCCEEDED(res)) << res;

    _small_icon = AtlLoadIconImage(IDI_MAINICON,
                                   LR_CREATEDIBSECTION,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON));

    _big_icon = AtlLoadIconImage(IDI_MAINICON,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));

    MainDialog main_dialog;
    main_dialog.DoModal();

    _module.Term();
}

} // namespace aspia

using namespace aspia;

LONG WINAPI CustomUnhandledExceptionFilter(PEXCEPTION_POINTERS info)
{
    WCHAR dump_dir_path[MAX_PATH - 14];

    // Получаем текущую директорию.
    if (!GetCurrentDirectoryW(ARRAYSIZE(dump_dir_path), dump_dir_path))
    {
        // Если не удалось получить текущую директорию, то пишем на диск С:.
        HRESULT hr = StringCchCopyW(dump_dir_path, ARRAYSIZE(dump_dir_path), L"c:");
        if (FAILED(hr))
            return EXCEPTION_EXECUTE_HANDLER;
    }

    GUID guid;

    // Генерируем уникальный GUID.
    if (CoCreateGuid(&guid) != S_OK)
        return EXCEPTION_EXECUTE_HANDLER;

    WCHAR uuid_string[64];

    // Конвертируем GUID в строку.
    if (!StringFromGUID2(guid, uuid_string, ARRAYSIZE(uuid_string)))
        return EXCEPTION_EXECUTE_HANDLER;

    WCHAR dump_file_path[MAX_PATH];

    // Генерируем имя файла в формате path\\pid.guid.dmp.
    if (FAILED(StringCchPrintfW(dump_file_path,
                                ARRAYSIZE(dump_file_path),
                                L"%s\\%u.%s.dmp",
                                dump_dir_path,
                                GetCurrentProcessId(),
                                uuid_string)))
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    // Создаем файл дампа.
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

    // Записываем дамп памяти в файл.
    MiniDumpWriteDump(GetCurrentProcess(),
                      GetCurrentProcessId(),
                      dump_file,
                      MiniDumpNormal,
                      &exception,
                      nullptr,
                      nullptr);

    // Записываем в лог путь к файлу дампа с сбрасываем логи на диск.
    LOG(ERROR) << "Memory dump written to: " << UTF8fromUNICODE(dump_file_path);
    google::FlushLogFiles(0);

    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[])
{
    // Парсим параметры коммандной строки.
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    FLAGS_log_dir = "C:\\TEMP";

    // Инициализируем библиотеку логгирования.
    google::InitGoogleLogging(argv[0]);

    SetUnhandledExceptionFilter(CustomUnhandledExceptionFilter);

    HRESULT res = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    CHECK(SUCCEEDED(res)) << res;

    std::wstring run_mode          = UNICODEfromUTF8(FLAGS_run_mode);
    std::wstring input_channel_id  = UNICODEfromUTF8(FLAGS_input_channel_id);
    std::wstring output_channel_id = UNICODEfromUTF8(FLAGS_output_channel_id);
    std::wstring service_id        = UNICODEfromUTF8(FLAGS_service_id);

    // Если процесс запущен без прав администратора.
    if (!Process::Current().HasAdminRights())
    {
        if (run_mode == kDesktopSessionSwitch)
        {
            DesktopSessionClient().Execute(input_channel_id, output_channel_id);
        }
        else
        {
            // Пытаемся запустить процесс через UAC.
            if (!Process::ElevateProcess())
            {
                // Если запустить не удалось, то запускаем с текущими правами.
                StartGUI();
            }
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
            DesktopSessionClient().Execute(input_channel_id, output_channel_id);
        }
        else
        {
            StartGUI();
        }
    }

    CoUninitialize();

    google::ShutdownGoogleLogging();
    gflags::ShutDownCommandLineFlags();

    return 0;
}
