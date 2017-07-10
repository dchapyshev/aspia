//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H
#define _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H

#include "client/client_session.h"
#include "client/file_request_sender_local.h"
#include "client/file_request_sender_remote.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/file_manager.h"

namespace aspia {

class ClientSessionFileTransfer :
    public ClientSession,
    private UiFileManager::Delegate
{
public:
    ClientSessionFileTransfer(const ClientConfig& config,
                              ClientSession::Delegate* delegate);
    ~ClientSessionFileTransfer();

private:
    // ClientSession implementation.
    void Send(const IOBuffer& buffer) override;

    // UiFileManager::Delegate implementation.
    void OnWindowClose() override;

    std::unique_ptr<UiFileManager> file_manager_;

    std::unique_ptr<FileRequestSenderLocal> local_sender_;
    std::unique_ptr<FileRequestSenderRemote> remote_sender_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionFileTransfer);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H
