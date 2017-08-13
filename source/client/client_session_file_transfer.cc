//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"

namespace aspia {

ClientSessionFileTransfer::ClientSessionFileTransfer(
    const ClientConfig& config,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : ClientSession(config, channel_proxy)
{
    remote_sender_ = std::make_unique<FileRequestSenderRemote>(channel_proxy);
    local_sender_ = std::make_unique<FileRequestSenderLocal>();

    file_manager_.reset(new FileManagerWindow(local_sender_->request_sender_proxy(),
                                              remote_sender_->request_sender_proxy(),
                                              this));
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    file_manager_.reset();
}

void ClientSessionFileTransfer::OnWindowClose()
{
    channel_proxy_->Disconnect();
}

} // namespace aspia
