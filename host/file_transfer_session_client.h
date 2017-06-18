//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__FILE_TRANSFER_SESSION_CLIENT_H
#define _ASPIA_HOST__FILE_TRANSFER_SESSION_CLIENT_H

#include "ipc/pipe_channel.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/file_status_dialog.h"

namespace aspia {

class FileTransferSessionClient :
    private PipeChannel::Delegate,
    private UiFileStatusDialog::Delegate
{
public:
    FileTransferSessionClient() = default;
    ~FileTransferSessionClient() = default;

    void Run(const std::wstring& input_channel_name,
             const std::wstring& output_channel_name);

private:
    // PipeChannel::Delegate implementation.
    void OnPipeChannelConnect(uint32_t user_data) override;
    void OnPipeChannelDisconnect() override;
    void OnPipeChannelMessage(const IOBuffer& buffer) override;

    // UiFileStatusWindow::Delegate implementation.
    void OnWindowClose() override;

    void WriteMessage(const proto::file_transfer::HostToClient& message);
    void WriteStatus(proto::Status status);

    bool ReadDriveListRequestMessage(const proto::DriveListRequest& drive_list_request);
    bool ReadDirectoryListRequestMessage(const proto::DirectoryListRequest& direcrory_list_request);
    bool ReadFileRequestMessage(const proto::FileRequest& file_request);
    bool ReadFileMessage(const proto::File& file);
    bool ReadCreateDirectoryRequest(const proto::CreateDirectoryRequest& create_directory_request);

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::mutex outgoing_lock_;

    std::unique_ptr<UiFileStatusDialog> status_dialog_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferSessionClient);
};

} // namespace aspia

#endif // _ASPIA_HOST__FILE_TRANSFER_SESSION_CLIENT_H
