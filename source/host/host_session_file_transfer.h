//
// PROJECT:         Aspia
// FILE:            host/host_session_file_transfer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H
#define _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H

#include "host/file_worker.h"
#include "ipc/pipe_channel.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class HostSessionFileTransfer
{
public:
    HostSessionFileTransfer() = default;
    ~HostSessionFileTransfer() = default;

    void Run(const std::wstring& channel_id);

private:
    void OnIpcChannelConnect(uint32_t user_data);
    void OnIpcChannelMessage(const QByteArray& buffer);

    void SendReply(const proto::file_transfer::Reply& reply);
    void OnReplySended();

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;

    FileWorker worker_;

    Q_DISABLE_COPY(HostSessionFileTransfer)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H
