//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/sas_injector.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/sas_injector.h"

#include <sas.h>

#include "base/service_manager.h"
#include "base/path.h"
#include "base/logging.h"
#include "base/unicode.h"
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

    // Получаем полный путь к исполняемому файлу.
    if (!GetPath(PathKey::FILE_EXE, &command_line))
        return;

    std::wstring service_id =
        ServiceManager::GenerateUniqueServiceId();

    std::wstring unique_name =
        ServiceManager::CreateUniqueServiceName(kSasServiceShortName, service_id);

    // Добавляем флаг запуска в виде службы.
    command_line.append(L" --run_mode=");
    command_line.append(kSasServiceSwitch);

    command_line.append(L" --service_id=");
    command_line.append(service_id);

    // Устанавливаем службу в системе.
    ServiceManager manager(ServiceManager::Create(command_line,
                                                  kSasServiceFullName,
                                                  unique_name));

    // Если служба установлена.
    if (manager.IsValid())
    {
        // Запускаем ее.
        manager.Start();
    }
}

void SasInjector::ExecuteService()
{
    // Запускаем службу для выполнения метода Worker().
    Run();

    // Удаляем службу.
    ServiceManager(ServiceName()).Remove();
}

void SasInjector::Worker()
{
    ScopedSasPolice sas_police;
    SendSAS(FALSE);
}

void SasInjector::OnStop()
{
    // Nothing
}

} // namespace aspia
