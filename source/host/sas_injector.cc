//
// PROJECT:         Aspia
// FILE:            host/sas_injector.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/sas_injector.h"
#include "base/scoped_thread_desktop.h"
#include "base/service_manager.h"
#include "base/version_helpers.h"
#include "base/files/base_paths.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "host/scoped_sas_police.h"

namespace aspia {

namespace {

constexpr wchar_t kSasServiceShortName[] = L"aspia-sas-service";
constexpr wchar_t kSasServiceFullName[] = L"Aspia SAS Injector";
constexpr DWORD kInvalidSessionId = 0xFFFFFFFF;

const wchar_t kServiceIdSwitch[] = L"service-id";

} // namespace

SasInjector::SasInjector()
    : Service(ServiceManager::CreateUniqueServiceName(
        kSasServiceShortName,
        CommandLine::ForCurrentProcess().GetSwitchValue(kServiceIdSwitch)))
{
    // Nothing
}

// static
void SasInjector::InjectSAS()
{
    if (!IsWindowsVistaOrGreater())
    {
        static constexpr wchar_t kWinlogonDesktopName[] = L"Winlogon";
        static constexpr wchar_t kSasWindowClassName[] = L"SAS window class";
        static constexpr wchar_t kSasWindowTitle[] = L"SAS window";

        Desktop winlogon_desktop(Desktop::GetDesktop(kWinlogonDesktopName));

        if (!winlogon_desktop.IsValid())
            return;

        ScopedThreadDesktop desktop;

        if (!desktop.SetThreadDesktop(std::move(winlogon_desktop)))
            return;

        HWND window = FindWindowW(kSasWindowClassName, kSasWindowTitle);
        if (!window)
            return;

        PostMessageW(window, WM_HOTKEY, 0, MAKELONG(MOD_ALT | MOD_CONTROL, VK_DELETE));
    }
    else // For Windows Vista and above.
    {
        std::experimental::filesystem::path program_path;

        if (!GetBasePath(BasePathKey::DIR_EXE, program_path))
            return;

        program_path.append(L"aspia_sas_injector.exe");

        std::wstring service_id = ServiceManager::GenerateUniqueServiceId();

        std::wstring unique_short_name =
            ServiceManager::CreateUniqueServiceName(kSasServiceShortName, service_id);

        std::wstring unique_full_name =
            ServiceManager::CreateUniqueServiceName(kSasServiceFullName, service_id);

        CommandLine command_line(program_path);
        command_line.AppendSwitch(kServiceIdSwitch, service_id);

        // Install the service in the system.
        std::unique_ptr<ServiceManager> manager =
            ServiceManager::Create(command_line,
                                   unique_full_name,
                                   unique_short_name,
                                   kSasServiceFullName);

        // If the service is installed.
        if (manager)
        {
            manager->Start();
        }
    }
}

void SasInjector::ExecuteService()
{
    Run();
    ServiceManager(ServiceName()).Remove();
}

void SasInjector::Worker()
{
    ScopedNativeLibrary wmsgapi(L"wmsgapi.dll");

    typedef DWORD(WINAPI *WmsgSendMessageFn)(DWORD session_id, UINT msg,
                                             WPARAM wParam, LPARAM lParam);

    WmsgSendMessageFn send_message_func =
        reinterpret_cast<WmsgSendMessageFn>(wmsgapi.GetFunctionPointer("WmsgSendMessage"));

    if (!send_message_func)
    {
        LOG(LS_ERROR) << "WmsgSendMessage() not found in wmsgapi.dll";
        return;
    }

    HMODULE kernel32_module = GetModuleHandleW(L"kernel32.dll");

    typedef DWORD(WINAPI *WTSGetActiveConsoleSessionIdFn)();

    WTSGetActiveConsoleSessionIdFn get_active_console_session_id_func =
        reinterpret_cast<WTSGetActiveConsoleSessionIdFn>(
            GetProcAddress(kernel32_module, "WTSGetActiveConsoleSessionId"));
    if (!get_active_console_session_id_func)
    {
        LOG(LS_ERROR) << "WTSGetActiveConsoleSessionId not found in kernel32.dll";
        return;
    }

    DWORD session_id = get_active_console_session_id_func();
    if (session_id == kInvalidSessionId)
    {
        PLOG(LS_ERROR) << "WTSGetActiveConsoleSessionId failed";
        return;
    }

    ScopedSasPolice sas_police;

    BOOL as_user_ = FALSE;
    send_message_func(session_id, 0x208, 0, reinterpret_cast<LPARAM>(&as_user_));
}

void SasInjector::OnStop()
{
    // Nothing
}

} // namespace aspia
