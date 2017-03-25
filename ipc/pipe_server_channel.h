//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_server_channel.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__PIPE_SERVER_CHANNEL_H
#define _ASPIA_IPC__PIPE_SERVER_CHANNEL_H

#include "aspia_config.h"

#include "base/scoped_handle.h"
#include "base/waitable_event.h"
#include "base/macros.h"
#include "ipc/pipe_channel.h"

namespace aspia {

class PipeServerChannel : public PipeChannel
{
public:
    ~PipeServerChannel();

    static PipeServerChannel* Create(std::wstring* input_channel_id,
                                     std::wstring* output_channel_id);

    bool Connect(const IncommingMessageCallback& incomming_message_callback);
    uint32_t GetPeerPid() const;

private:
    PipeServerChannel(HANDLE read_pipe, HANDLE write_pipe);

    uint32_t pid_;

    DISALLOW_COPY_AND_ASSIGN(PipeServerChannel);
};

} // namespace aspia

#endif // _ASPIA_IPC__PIPE_SERVER_CHANNEL_H
