//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_system_info.h"
#include "base/process/process.h"
#include "base/printer_enumerator.h"
#include "base/service_enumerator.h"
#include "base/user_enumerator.h"
#include "base/user_group_enumerator.h"
#include "base/session_enumerator.h"
#include "ipc/pipe_channel_proxy.h"
#include "network/network_adapter_enumerator.h"
#include "network/open_connection_enumerator.h"
#include "network/share_enumerator.h"
#include "network/route_enumerator.h"
#include "proto/auth_session.pb.h"
#include "proto/system_info_session.pb.h"
#include "proto/system_info_session_message.pb.h"
#include "protocol/message_serialization.h"
#include "protocol/system_info_constants.h"

namespace aspia {

namespace {

std::string GetSummary()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiBios()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiSystem()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiMotherboard()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiChassis()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiCaches()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiProcessors()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiMemoryDevices()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiSystemSlots()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiPortConnectors()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiOnboardDevices()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiBuildinPointing()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetDmiPortableBattery()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetCPU()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetStorageOpticalDrives()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetStorageATA()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetStorageSMART()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetWindowsVideo()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetMonitor()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetOpenGL()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetPowerOptions()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetPrinters()
{
    system_info::Printers message;

    for (PrinterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::Printers::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_is_default(enumerator.IsDefault());
        item->set_is_shared(enumerator.IsShared());
        item->set_share_name(enumerator.GetShareName());
        item->set_port_name(enumerator.GetPortName());
        item->set_driver_name(enumerator.GetDriverName());
        item->set_device_name(enumerator.GetDeviceName());
        item->set_print_processor(enumerator.GetPrintProcessor());
        item->set_data_type(enumerator.GetDataType());
        item->set_server_name(enumerator.GetServerName());
        item->set_location(enumerator.GetLocation());
        item->set_comment(enumerator.GetComment());
        item->set_jobs_count(enumerator.GetJobsCount());
        item->set_paper_width(enumerator.GetPaperWidth());
        item->set_paper_length(enumerator.GetPaperLength());
        item->set_print_quality(enumerator.GetPrintQuality());

        switch (enumerator.GetOrientation())
        {
            case PrinterEnumerator::Orientation::PORTRAIT:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_PORTRAIT);
                break;

            case PrinterEnumerator::Orientation::LANDSCAPE:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_LANDSCAPE);
                break;

            default:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_UNKNOWN);
                break;
        }
    }

    return message.SerializeAsString();
}

std::string GetAllDevices()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetUnknownDevices()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetPrograms()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetUpdates()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetServices()
{
    system_info::Services message;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::SERVICES);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        system_info::Services::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_display_name(enumerator.GetDisplayName());
        item->set_description(enumerator.GetDescription());

        switch (enumerator.GetStatus())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                item->set_status(system_info::Services::Item::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                item->set_status(system_info::Services::Item::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                item->set_status(system_info::Services::Item::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                item->set_status(system_info::Services::Item::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                item->set_status(system_info::Services::Item::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                item->set_status(system_info::Services::Item::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                item->set_status(system_info::Services::Item::STATUS_STOPPED);
                break;

            default:
                item->set_status(system_info::Services::Item::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.GetStartupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(enumerator.GetBinaryPath());
        item->set_start_name(enumerator.GetStartName());
    }

    return message.SerializeAsString();
}

std::string GetDrivers()
{
    system_info::Services message;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::DRIVERS);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        system_info::Services::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_display_name(enumerator.GetDisplayName());
        item->set_description(enumerator.GetDescription());

        switch (enumerator.GetStatus())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                item->set_status(system_info::Services::Item::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                item->set_status(system_info::Services::Item::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                item->set_status(system_info::Services::Item::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                item->set_status(system_info::Services::Item::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                item->set_status(system_info::Services::Item::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                item->set_status(system_info::Services::Item::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                item->set_status(system_info::Services::Item::STATUS_STOPPED);
                break;

            default:
                item->set_status(system_info::Services::Item::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.GetStartupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(enumerator.GetBinaryPath());
        item->set_start_name(enumerator.GetStartName());
    }

    return message.SerializeAsString();
}

std::string GetProcesses()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetLicenses()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetNetworkCards()
{
    system_info::NetworkCards message;

    for (NetworkAdapterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::NetworkCards::Item* item = message.add_item();

        item->set_adapter_name(enumerator.GetAdapterName());
        item->set_connection_name(enumerator.GetConnectionName());
        item->set_interface_type(enumerator.GetInterfaceType());
        item->set_mtu(enumerator.GetMtu());
        item->set_speed(enumerator.GetSpeed());
        item->set_mac_address(enumerator.GetMacAddress());
        item->set_is_wins_enabled(enumerator.IsWinsEnabled());
        item->set_primary_wins_server(enumerator.GetPrimaryWinsServer());
        item->set_secondary_wins_server(enumerator.GetSecondaryWinsServer());
        item->set_is_dhcp_enabled(enumerator.IsDhcpEnabled());

        for (NetworkAdapterEnumerator::IpAddressEnumerator ip_address_enumerator(enumerator);
             !ip_address_enumerator.IsAtEnd();
             ip_address_enumerator.Advance())
        {
            system_info::NetworkCards::Item::IpAddress* address = item->add_ip_address();

            address->set_address(ip_address_enumerator.GetIpAddress());
            address->set_mask(ip_address_enumerator.GetIpMask());
        }

        for (NetworkAdapterEnumerator::GatewayEnumerator gateway_enumerator(enumerator);
             !gateway_enumerator.IsAtEnd();
             gateway_enumerator.Advance())
        {
            item->add_gateway_address(gateway_enumerator.GetAddress());
        }

        for (NetworkAdapterEnumerator::DhcpEnumerator dhcp_enumerator(enumerator);
             !dhcp_enumerator.IsAtEnd();
             dhcp_enumerator.Advance())
        {
            item->add_dhcp_server_address(dhcp_enumerator.GetAddress());
        }

        for (NetworkAdapterEnumerator::DnsEnumerator dns_enumerator(enumerator);
             !dns_enumerator.IsAtEnd();
             dns_enumerator.Advance())
        {
            item->add_dns_server_address(dns_enumerator.GetAddress());
        }
    }

    return message.SerializeAsString();
}

std::string GetRasConnections()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetOpenConnections()
{
    system_info::OpenConnections message;

    for (OpenConnectionEnumerator enumerator(OpenConnectionEnumerator::Type::TCP);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        system_info::OpenConnections::Item* item = message.add_item();

        item->set_process_name(enumerator.GetProcessName());
        item->set_local_address(enumerator.GetLocalAddress());
        item->set_remote_address(enumerator.GetRemoteAddress());
        item->set_local_port(enumerator.GetLocalPort());
        item->set_remote_port(enumerator.GetRemotePort());
        item->set_state(enumerator.GetState());
    }

    for (OpenConnectionEnumerator enumerator(OpenConnectionEnumerator::Type::UDP);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        system_info::OpenConnections::Item* item = message.add_item();

        item->set_process_name(enumerator.GetProcessName());
        item->set_local_address(enumerator.GetLocalAddress());
        item->set_remote_address(enumerator.GetRemoteAddress());
        item->set_local_port(enumerator.GetLocalPort());
        item->set_remote_port(enumerator.GetRemotePort());
        item->set_state(enumerator.GetState());
    }

    return message.SerializeAsString();
}

std::string GetSharedResources()
{
    system_info::SharedResources message;

    for (ShareEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::SharedResources::Item* item = message.add_item();

        item->set_name(enumerator.GetName());

        switch (enumerator.GetType())
        {
            case ShareEnumerator::Type::DISK:
                item->set_type(system_info::SharedResources::Item::TYPE_DISK);
                break;

            case ShareEnumerator::Type::PRINTER:
                item->set_type(system_info::SharedResources::Item::TYPE_PRINTER);
                break;

            case ShareEnumerator::Type::DEVICE:
                item->set_type(system_info::SharedResources::Item::TYPE_DEVICE);
                break;

            case ShareEnumerator::Type::IPC:
                item->set_type(system_info::SharedResources::Item::TYPE_IPC);
                break;

            case ShareEnumerator::Type::SPECIAL:
                item->set_type(system_info::SharedResources::Item::TYPE_SPECIAL);
                break;

            case ShareEnumerator::Type::TEMPORARY:
                item->set_type(system_info::SharedResources::Item::TYPE_TEMPORARY);
                break;

            default:
                item->set_type(system_info::SharedResources::Item::TYPE_UNKNOWN);
                break;
        }

        item->set_current_uses(enumerator.GetCurrentUses());
        item->set_maximum_uses(enumerator.GetMaximumUses());
    }

    return message.SerializeAsString();
}

std::string GetOpenFiles()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetRoutes()
{
    system_info::Routes message;

    for (RouteEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::Routes::Item* item = message.add_item();

        item->set_destonation(enumerator.GetDestonation());
        item->set_mask(enumerator.GetMask());
        item->set_gateway(enumerator.GetGateway());
        item->set_metric(enumerator.GetMetric());
    }

    return message.SerializeAsString();
}

std::string GetRegistrationInformation()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetTaskScheduler()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetUsers()
{
    system_info::Users message;

    for (UserEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::Users::Item* item = message.add_item();

        item->set_full_name(enumerator.GetFullName());
        item->set_comment(enumerator.GetComment());
        item->set_is_disabled(enumerator.IsDisabled());
        item->set_is_password_cant_change(enumerator.IsPasswordCantChange());
        item->set_is_password_expired(enumerator.IsPasswordExpired());
        item->set_is_dont_expire_password(enumerator.IsDontExpirePassword());
        item->set_is_lockout(enumerator.IsLockout());
        item->set_number_logons(enumerator.GetNumberLogons());
        item->set_bad_password_count(enumerator.GetBadPasswordCount());
        item->set_last_logon_time(enumerator.GetLastLogonTime());
        item->set_user_id(enumerator.GetUserId());
        item->set_profile_directory(enumerator.GetProfileDirectory());
        item->set_home_directory(enumerator.GetHomeDirectory());
        item->set_script_path(enumerator.GetScriptPath());
        item->set_country_code(enumerator.GetCountryCode());
        item->set_codepage(enumerator.GetCodePage());
        item->set_primary_group_id(enumerator.GetPrimaryGroupId());
    }

    return message.SerializeAsString();
}

std::string GetUserGroups()
{
    system_info::UserGroups message;

    for (UserGroupEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::UserGroups::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_comment(enumerator.GetComment());
    }

    return message.SerializeAsString();
}

std::string GetActiveSessions()
{
    system_info::Sessions message;

    for (SessionEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::Sessions::Item* item = message.add_item();

        item->set_user_name(enumerator.GetUserName());
        item->set_domain_name(enumerator.GetDomainName());
        item->set_session_id(enumerator.GetSessionId());
        item->set_connect_state(enumerator.GetConnectState());
        item->set_client_name(enumerator.GetClientName());
        item->set_winstation_name(enumerator.GetWinStationName());
    }

    return message.SerializeAsString();
}

std::string GetEnvironmentVariables()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetEventLogApplications()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetEventLogSecurity()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

std::string GetEventLogSystem()
{
    LOG(WARNING) << "Not implemented yet";
    return std::string();
}

} // namespace

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
