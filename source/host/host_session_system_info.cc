//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_system_info.h"
#include "base/process/process.h"
#include "ipc/pipe_channel_proxy.h"
#include "proto/auth_session.pb.h"
#include "proto/system_info_session.pb.h"
#include "protocol/message_serialization.h"
#include "protocol/system_info_constants.h"

namespace aspia {

void HostSessionSystemInfo::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (!ipc_channel_)
        return;

    ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

    uint32_t user_data = Process::Current().Pid();

    if (ipc_channel_->Connect(user_data))
    {
        OnIpcChannelConnect(user_data);

        // Waiting for the connection to close.
        ipc_channel_proxy_->WaitForDisconnect();
    }
}

void HostSessionSystemInfo::RegisterSupportedCategories()
{
#if 0
    map_.emplace(system_info::kSummary, std::bind(&GetSummary));

    map_.emplace(system_info::hardware::dmi::kBIOS, std::bind(&GetDmiBios));
    map_.emplace(system_info::hardware::dmi::kSystem, std::bind(&GetDmiSystem));
    map_.emplace(system_info::hardware::dmi::kMotherboard, std::bind(&GetDmiMotherboard));
    map_.emplace(system_info::hardware::dmi::kChassis, std::bind(&GetDmiChassis));
    map_.emplace(system_info::hardware::dmi::kCaches, std::bind(&GetDmiCaches));
    map_.emplace(system_info::hardware::dmi::kProcessors, std::bind(&GetDmiProcessors));
    map_.emplace(system_info::hardware::dmi::kMemoryDevices, std::bind(&GetDmiMemoryDevices));
    map_.emplace(system_info::hardware::dmi::kSystemSlots, std::bind(&GetDmiSystemSlots));
    map_.emplace(system_info::hardware::dmi::kPortConnectors, std::bind(&GetDmiPortConnectors));
    map_.emplace(system_info::hardware::dmi::kOnboardDevices, std::bind(&GetDmiOnboardDevices));
    map_.emplace(system_info::hardware::dmi::kBuildinPointing, std::bind(&GetDmiBuildinPointing));
    map_.emplace(system_info::hardware::dmi::kPortableBattery, std::bind(&GetDmiPortableBattery));

    map_.emplace(system_info::hardware::kCPU, std::bind(&GetCPU));

    map_.emplace(system_info::hardware::storage::kOpticalDrives, std::bind(&GetStorageOpticalDrives));
    map_.emplace(system_info::hardware::storage::kATA, std::bind(&GetStorageATA));
    map_.emplace(system_info::hardware::storage::kSMART, std::bind(&GetStorageSMART));

    map_.emplace(system_info::hardware::display::kWindowsVideo, std::bind(&GetWindowsVideo));
    map_.emplace(system_info::hardware::display::kMonitor, std::bind(&GetMonitor));
    map_.emplace(system_info::hardware::display::kOpenGL, std::bind(&GetOpenGL));

    map_.emplace(system_info::hardware::kPowerOptions, std::bind(&GetPowerOptions));
    map_.emplace(system_info::hardware::kPrinters, std::bind(&GetPrinters));

    map_.emplace(system_info::hardware::windows_devices::kAll, std::bind(&GetAllDevices));
    map_.emplace(system_info::hardware::windows_devices::kUnknown, std::bind(&GetUnknownDevices));

    map_.emplace(system_info::software::kPrograms, std::bind(&GetPrograms));
    map_.emplace(system_info::software::kUpdates, std::bind(&GetUpdates));
    map_.emplace(system_info::software::kServices, std::bind(&GetServices));
    map_.emplace(system_info::software::kDrivers, std::bind(&GetDrivers));
    map_.emplace(system_info::software::kProcesses, std::bind(&GetProcesses));
    map_.emplace(system_info::software::kLicenses, std::bind(&GetLicenses));

    map_.emplace(system_info::network::kNetworkCards, std::bind(&GetNetworkCards));
    map_.emplace(system_info::network::kRASConnections, std::bind(&GetRasConnections));
    map_.emplace(system_info::network::kOpenConnections, std::bind(&GetOpenConnections));
    map_.emplace(system_info::network::kSharedResources, std::bind(&GetSharedResources));
    map_.emplace(system_info::network::kOpenFiles, std::bind(&GetOpenFiles));
    map_.emplace(system_info::network::kRoutes, std::bind(&GetRoutes));

    map_.emplace(system_info::operating_system::kRegistrationInformation, std::bind(&GetRegistrationInformation));
    map_.emplace(system_info::operating_system::kTaskScheduler, std::bind(&GetTaskScheduler));

    map_.emplace(system_info::operating_system::users_and_groups::kUsers, std::bind(&GetUsers));
    map_.emplace(system_info::operating_system::users_and_groups::kUserGroups, std::bind(&GetUserGroups));
    map_.emplace(system_info::operating_system::users_and_groups::kActiveSessions, std::bind(&GetActiveSessions));

    map_.emplace(system_info::operating_system::kEnvironmentVariables, std::bind(&GetEnvironmentVariables));

    map_.emplace(system_info::operating_system::event_logs::kApplications, std::bind(&GetEventLogApplications));
    map_.emplace(system_info::operating_system::event_logs::kSecurity, std::bind(&GetEventLogSecurity));
    map_.emplace(system_info::operating_system::event_logs::kSystem, std::bind(&GetEventLogSystem));
#endif
}

void HostSessionSystemInfo::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::SessionType session_type = static_cast<proto::SessionType>(user_data);

    DCHECK(session_type == proto::SESSION_TYPE_SYSTEM_INFO);

    RegisterSupportedCategories();

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionSystemInfo::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionSystemInfo::OnIpcChannelMessage(const IOBuffer& buffer)
{
    proto::system_info::ClientToHost request;

    if (ParseMessage(buffer, request))
    {
        const std::string& guid = request.guid();

        if (guid.length() == system_info::kGuidLength)
        {
            proto::system_info::HostToClient reply;

            reply.set_guid(guid);

            // Looking for a category by GUID.
            const auto category = map_.find(guid);
            if (category != map_.end())
            {
                // TODO: COMPRESSOR_ZLIB support.
                reply.set_compressor(proto::system_info::HostToClient::COMPRESSOR_RAW);
                reply.set_data(category->second());
            }

            // Sending response to the request.
            ipc_channel_proxy_->Send(
                SerializeMessage<IOBuffer>(reply),
                std::bind(&HostSessionSystemInfo::OnIpcChannelMessageSended, this));
            return;
        }
    }

    ipc_channel_proxy_->Disconnect();
}

void HostSessionSystemInfo::OnIpcChannelMessageSended()
{
    // The response to the request was sent. Now we can receive the following request.
    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionSystemInfo::OnIpcChannelMessage, this, std::placeholders::_1));
}

} // namespace aspia
