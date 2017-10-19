//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_system_info.h"
#include "proto/system_info_session.pb.h"
#include "protocol/message_serialization.h"

namespace aspia {

static const size_t kGuidLength = 36;

ClientSessionSystemInfo::ClientSessionSystemInfo(
    const ClientConfig& config,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : ClientSession(config, channel_proxy)
{
    window_.reset(new SystemInfoWindow(this));
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    window_.reset();
}

void ClientSessionSystemInfo::OnCategoryRequest(const char* guid,
                                                ParseCallback parse_callback,
                                                std::shared_ptr<OutputProxy> output)
{
    DCHECK(parse_callback);
    DCHECK(output);
    DCHECK(guid);

    last_guid_ = guid;
    parse_callback_ = parse_callback;
    output_ = std::move(output);

    DCHECK(last_guid_.length() == kGuidLength);

    proto::system_info::ClientToHost message;
    message.set_guid(guid);

    channel_proxy_->Send(SerializeMessage<IOBuffer>(message),
                         std::bind(&ClientSessionSystemInfo::OnMessageSended, this));
}

void ClientSessionSystemInfo::OnWindowClose()
{
    channel_proxy_->Disconnect();
}

void ClientSessionSystemInfo::OnMessageSended()
{
    channel_proxy_->Receive(std::bind(
        &ClientSessionSystemInfo::OnMessageReceived, this, std::placeholders::_1));
}

void ClientSessionSystemInfo::OnMessageReceived(const IOBuffer& buffer)
{
    DCHECK(last_guid_.length() == kGuidLength);

    proto::system_info::HostToClient message;

    if (ParseMessage(buffer, message))
    {
        const std::string& guid = message.guid();

        if (guid.length() == kGuidLength && guid == last_guid_)
        {
            parse_callback_(output_, message.data());
            parse_callback_ = nullptr;
            output_ = nullptr;
            return;
        }
    }

    channel_proxy_->Disconnect();
}

} // namespace aspia
