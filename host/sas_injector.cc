//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/sas_injector.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/sas_injector.h"

#include <sas.h>

#include "base/service_manager.h"
#include "base/path.h"
#include "base/logging.h"
#include "base/unicode.h"
#include "base/scoped_native_library.h"
#include "host/scoped_sas_police.h"

namespace aspia {

static const WCHAR kSasServiceShortName[] = L"aspia-sas-service";
static const WCHAR kSasServiceFullName[] = L"Aspia SAS Injector";

SasInjector::SasInjector(const std::wstring& service_id) :
    Service(ServiceManager::CreateUniqueServiceName(kSasServiceShortName, service_id))
{
    // Nothing
}

// static
void SasInjector::InjectSAS()
{
    std::wstring command_line;

    if (!GetPathW(PathKey::FILE_EXE, command_line))
        return;

    std::wstring service_id =
        ServiceManager::GenerateUniqueServiceId();

    std::wstring unique_name =
        ServiceManager::CreateUniqueServiceName(kSasServiceShortName, service_id);

    command_line.append(L" --run_mode=");
    command_line.append(kSasServiceSwitch);

    command_line.append(L" --service_id=");
    command_line.append(service_id);

    // Install the service in the system.
    ServiceManager manager(ServiceManager::Create(command_line,
                                                  kSasServiceFullName,
                                                  unique_name));

    // If the service is installed.
    if (manager.IsValid())
        manager.Start();
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

    WTSGetActiveConsoleSessionIdFn get_active_console_session_id_ =
        reinterpret_cast<WTSGetActiveConsoleSessionIdFn>(
            GetProcAddress(kernel32_module, "WTSGetActiveConsoleSessionId"));
    if (!get_active_console_session_id_)
    {
        LOG(ERROR) << "WTSGetActiveConsoleSessionId() not found in kernel32.dll";
        return;
    }

    DWORD session_id = get_active_console_session_id_();
    if (session_id == 0xFFFFFFFF)
    {
        LOG(ERROR) << "WTSGetActiveConsoleSessionId() failed: " << GetLastError();
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
