//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_client_channel.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__PIPE_CLIENT_CHANNEL_H
#define _ASPIA_IPC__PIPE_CLIENT_CHANNEL_H

#include "aspia_config.h"

#include "base/scoped_handle.h"
#include "base/waitable_event.h"
#include "base/macros.h"
#include "ipc/pipe_channel.h"

namespace aspia {

class PipeClientChannel : public PipeChannel
{
public:
    PipeClientChannel();
    ~PipeClientChannel();

    bool Connect(const std::wstring& input_channel_id,
                 const std::wstring& output_channel_id,
                 const IncommingMessageCallback& incomming_message_callback);

private:
    DISALLOW_COPY_AND_ASSIGN(PipeClientChannel);
};

} // namespace aspia

#endif // _ASPIA_IPC__PIPE_CLIENT_CHANNEL_H
