//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/sas_injector.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/sas_injector.h"
#include "base/scoped_thread_desktop.h"
#include "base/service_manager.h"
#include "base/version_helpers.h"
#include "base/path.h"
#include "base/logging.h"
#include "base/strings/unicode.h"
#include "base/scoped_native_library.h"
#include "host/scoped_sas_police.h"

namespace aspia {

static const WCHAR kSasServiceShortName[] = L"aspia-sas-service";
static const WCHAR kSasServiceFullName[] = L"Aspia SAS Injector";
static const DWORD kInvalidSessionId = 0xFFFFFFFF;

SasInjector::SasInjector(const std::wstring& service_id) :
    Service(ServiceManager::CreateUniqueServiceName(kSasServiceShortName, service_id))
{
    // Nothing
}

// static
void SasInjector::InjectSAS()
{
    if (!IsWindowsVistaOrGreater())
    {
        const wchar_t kWinlogonDesktopName[] = L"Winlogon";
        const wchar_t kSasWindowClassName[] = L"SAS window class";
        const wchar_t kSasWindowTitle[] = L"SAS window";

        Desktop winlogon_desktop(Desktop::GetDesktop(kWinlogonDesktopName));

        if (!winlogon_desktop.IsValid())
            return;

        ScopedThreadDesktop desktop;

        if (!desktop.SetThreadDesktop(std::move(winlogon_desktop)))
            return;

        HWND window = FindWindowW(kSasWindowClassName, kSasWindowTitle);
        if (!window)
            return;

        PostMessageW(window,
                     WM_HOTKEY,
                     0,
                     MAKELONG(MOD_ALT | MOD_CONTROL, VK_DELETE));
    }
    else // For Windows Vista and above.
    {
        std::experimental::filesystem::path path;

        if (!GetBasePath(PathKey::FILE_EXE, path))
            return;

        std::wstring service_id =
            ServiceManager::GenerateUniqueServiceId();

        std::wstring unique_short_name =
            ServiceManager::CreateUniqueServiceName(kSasServiceShortName, service_id);

        std::wstring unique_full_name =
            ServiceManager::CreateUniqueServiceName(kSasServiceFullName, service_id);

        std::wstring command_line;

        command_line.assign(path);
        command_line.append(L" --run_mode=");
        command_line.append(kSasServiceSwitch);

        command_line.append(L" --service_id=");
        command_line.append(service_id);

        // Install the service in the system.
        ServiceManager manager(ServiceManager::Create(command_line,
                                                      unique_full_name,
                                                      unique_short_name));

        // If the service is installed.
        if (manager.IsValid())
            manager.Start();
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

    typedef DWORD(WINAPI *WmsgSendMessageFn)(DWORD session_id, UINT msg, WPARAM wParam, LPARAM lParam);

    WmsgSendMessageFn send_message_func =
        reinterpret_cast<WmsgSendMessageFn>(wmsgapi.GetFunctionPointer("WmsgSendMessage"));

    if (!send_message_func)
    {
        LOG(ERROR) << "WmsgSendMessage() not found in wmsgapi.dll";
        return;
    }

    HMODULE kernel32_module = GetModuleHandleW(L"kernel32.dll");

    typedef DWORD(WINAPI *WTSGetActiveConsoleSessionIdFn)();

    WTSGetActiveConsoleSessionIdFn get_active_console_session_id_func =
        reinterpret_cast<WTSGetActiveConsoleSessionIdFn>(
            GetProcAddress(kernel32_module, "WTSGetActiveConsoleSessionId"));
    if (!get_active_console_session_id_func)
    {
        LOG(ERROR) << "WTSGetActiveConsoleSessionId() not found in kernel32.dll";
        return;
    }

    DWORD session_id = get_active_console_session_id_func();
    if (session_id == kInvalidSessionId)
    {
        LOG(ERROR) << "WTSGetActiveConsoleSessionId() failed: "
                   << GetLastSystemErrorString();
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
