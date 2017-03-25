//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_service.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_service.h"
#include "base/service_manager.h"
#include "base/unicode.h"
#include "base/path.h"

namespace aspia {

static const WCHAR kHostServiceShortName[] = L"aspia-remote-desktop-host";
static const WCHAR kHostServiceFullName[] = L"Aspia Remote Desktop Host";

HostService::HostService() :
    Service(kHostServiceShortName)
{
    // Nothing
}

void HostService::Worker()
{
    Host::EventCallback event_callback =
        std::bind(&HostService::OnServerEvent, this, std::placeholders::_1);

    // Создаем TCP-сервер.
    server_.reset(new ServerTCP<Host>(kDefaultHostTcpPort, event_callback));

    // Ждем пока сервер не будет остановлен.
    server_->Wait();
}

void HostService::OnStop()
{
    if (server_)
        server_->Stop();
}

void HostService::OnServerEvent(Host::SessionEvent event)
{
    UNREFERENCED_PARAMETER(event);

    // TODO: Отправка событий в GUI.
}

// static
bool HostService::IsInstalled()
{
    return ServiceManager(kHostServiceShortName).IsValid();
}

// static
bool HostService::Install()
{
    std::wstring command_line;

    if (!GetPath(PathKey::FILE_EXE, &command_line))
        return false;

    command_line.append(L" --run_mode=");
    command_line.append(kHostServiceSwitch);

    ServiceManager manager(ServiceManager::Create(command_line,
                                                  kHostServiceFullName,
                                                  kHostServiceShortName,
                                                  kHostServiceFullName));
    if (manager.IsValid())
        return manager.Start();

    return false;
}

// static
bool HostService::Remove()
{
    ServiceManager manager(kHostServiceShortName);
    manager.Stop();
    return manager.Remove();
}

} // namespace aspia
