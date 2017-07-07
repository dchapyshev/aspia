//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"

namespace aspia {

ClientSessionFileTransfer::ClientSessionFileTransfer(const ClientConfig& config,
                                                     ClientSession::Delegate* delegate) :
    ClientSession(config, delegate)
{
    remote_sender_ = std::make_unique<FileRequestSenderRemote>(delegate);
    local_sender_ = std::make_unique<FileRequestSenderLocal>();

    file_manager_.reset(new UiFileManager(local_sender_->request_sender_proxy(), remote_sender_->request_sender_proxy(), this));
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    file_manager_.reset();
}

void ClientSessionFileTransfer::Send(const IOBuffer& buffer)
{
    remote_sender_->ReadIncommingMessage(buffer);
}

void ClientSessionFileTransfer::OnWindowClose()
{
    delegate_->OnSessionTerminate();
}

} // namespace aspia
