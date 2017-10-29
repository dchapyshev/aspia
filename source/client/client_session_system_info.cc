//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_system_info.h"
#include "proto/system_info_session.pb.h"
#include "protocol/message_serialization.h"
#include "ui/system_info/document_creater_proxy.h"

namespace aspia {

static const size_t kGuidLength = 36;

ClientSessionSystemInfo::ClientSessionSystemInfo(
    const ClientConfig& config,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : ClientSession(config, channel_proxy)
{
    window_.reset(new SystemInfoWindow(this, this));
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    window_.reset();
}

void ClientSessionSystemInfo::OnRequest(const std::string& guid,
                                        std::shared_ptr<DocumentCreaterProxy> creater)
{
    document_creater_ = std::move(creater);

    proto::system_info::ClientToHost message;
    message.set_guid(guid);

    channel_proxy_->Send(SerializeMessage<IOBuffer>(message),
                         std::bind(&ClientSessionSystemInfo::OnRequestSended, this));
}

void ClientSessionSystemInfo::OnWindowClose()
{
    channel_proxy_->Disconnect();
}

void ClientSessionSystemInfo::OnRequestSended()
{
    channel_proxy_->Receive(std::bind(
        &ClientSessionSystemInfo::OnReplyReceived, this, std::placeholders::_1));
}

void ClientSessionSystemInfo::OnReplyReceived(const IOBuffer& buffer)
{
    proto::system_info::HostToClient message;

    if (ParseMessage(buffer, message))
    {
        const std::string& guid = message.guid();

        if (guid.length() == kGuidLength)
        {
            std::shared_ptr<std::string> data =
                std::shared_ptr<std::string>(message.release_data());

            document_creater_->Parse(std::move(data));
        }
    }

    channel_proxy_->Disconnect();
}

} // namespace aspia
