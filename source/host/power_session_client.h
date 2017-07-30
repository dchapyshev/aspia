//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/power_session_client.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__POWER_SESSION_CLIENT_H
#define _ASPIA_HOST__POWER_SESSION_CLIENT_H

#include "ipc/pipe_channel.h"

namespace aspia {

class PowerSessionClient
{
public:
    PowerSessionClient() = default;
    ~PowerSessionClient() = default;

    void Run(const std::wstring& channel_id);

private:
    void OnIpcChannelConnect(uint32_t user_data);
    void OnIpcChannelMessage(std::unique_ptr<IOBuffer> buffer);

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;

    DISALLOW_COPY_AND_ASSIGN(PowerSessionClient);
};

} // namespace aspia

#endif // _ASPIA_HOST__POWER_SESSION_CLIENT_H
