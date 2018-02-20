//
// PROJECT:         Aspia
// FILE:            host/sas_injector.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/sas_injector.h"

#include <sas.h>

#include "base/scoped_thread_desktop.h"
#include "base/service_manager.h"
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

void SasInjector::ExecuteService()
{
    Run();
    ServiceManager(ServiceName()).Remove();
}

void SasInjector::Worker()
{
    SendSAS(FALSE);
}

void SasInjector::OnStop()
{
    // Nothing
}

} // namespace aspia
