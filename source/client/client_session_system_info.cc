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
    map_ = CreateCategoryMap();
    window_.reset(new SystemInfoWindow(this));
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    window_.reset();
}

void ClientSessionSystemInfo::OnRequest(GuidList list, std::shared_ptr<OutputProxy> output)
{
    DCHECK(output);
    DCHECK(!list.empty());

    guid_list_ = std::move(list);
    output_ = std::move(output);

    output_->StartDocument();

    SendRequest();
}

void ClientSessionSystemInfo::OnWindowClose()
{
    channel_proxy_->Disconnect();
}

void ClientSessionSystemInfo::SendRequest()
{
    DCHECK(!guid_list_.empty());
    DCHECK(guid_list_.front().length() == kGuidLength);

    proto::system_info::ClientToHost message;
    message.set_guid(guid_list_.front());

    channel_proxy_->Send(SerializeMessage<IOBuffer>(message),
                         std::bind(&ClientSessionSystemInfo::OnRequestSended, this));
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

        if (guid.length() == kGuidLength && guid == guid_list_.front())
        {
            // Looking for a category by GUID.
            const auto category = map_.find(guid);
            if (category != map_.end())
            {
                category->second->Parse(output_, message.data());
                guid_list_.pop_front();

                if (guid_list_.empty())
                {
                    output_->EndDocument();
                }
                else
                {
                    SendRequest();
                }

                return;
            }
        }
    }

    channel_proxy_->Disconnect();
}

} // namespace aspia
