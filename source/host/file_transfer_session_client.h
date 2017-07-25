//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__FILE_TRANSFER_SESSION_CLIENT_H
#define _ASPIA_HOST__FILE_TRANSFER_SESSION_CLIENT_H

#include "ipc/pipe_channel.h"
#include "proto/file_transfer_session.pb.h"
#include "protocol/file_depacketizer.h"
#include "protocol/file_packetizer.h"
#include "ui/file_status_dialog.h"

namespace aspia {

class FileTransferSessionClient
{
public:
    FileTransferSessionClient() = default;
    ~FileTransferSessionClient() = default;

    void Run(const std::wstring& channel_id);

private:
    // PipeChannel::Delegate implementation.
    void OnPipeChannelConnect(uint32_t user_data);
    void OnPipeChannelDisconnect();
    void OnPipeChannelMessage(IOBuffer buffer);

    void SendReply(const proto::file_transfer::HostToClient& reply);

    void ReadDriveListRequest();
    void ReadFileListRequest(const proto::FileListRequest& request);
    void ReadCreateDirectoryRequest(const proto::CreateDirectoryRequest& request);
    void ReadDirectorySizeRequest(const proto::DirectorySizeRequest& request);
    void ReadRenameRequest(const proto::RenameRequest& request);
    void ReadRemoveRequest(const proto::RemoveRequest& request);
    void ReadFileUploadRequest(const proto::FileUploadRequest& request);
    bool ReadFileUploadDataRequest(const proto::FilePacket& file_packet);
    void ReadFileDownloadRequest(const proto::FileDownloadRequest& request);
    bool ReadFileDownloadDataRequest();

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;

    std::unique_ptr<FileDepacketizer> file_depacketizer_;
    std::unique_ptr<FilePacketizer> file_packetizer_;

    std::unique_ptr<UiFileStatusDialog> status_dialog_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferSessionClient);
};

} // namespace aspia

#endif // _ASPIA_HOST__FILE_TRANSFER_SESSION_CLIENT_H
