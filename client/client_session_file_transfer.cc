//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"

namespace aspia {

ClientSessionFileTransfer::ClientSessionFileTransfer(const ClientConfig& config,
                                                     ClientSession::Delegate* delegate) :
    ClientSession(config, delegate),
    remote_sender_(delegate)
{
    file_manager_.reset(new UiFileManager(&local_sender_, &remote_sender_, this));
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    file_manager_.reset();
}

void ClientSessionFileTransfer::Send(const IOBuffer& buffer)
{
    remote_sender_.ReadIncommingMessage(buffer);
}

void ClientSessionFileTransfer::OnWindowClose()
{
    delegate_->OnSessionTerminate();
}

} // namespace aspia
