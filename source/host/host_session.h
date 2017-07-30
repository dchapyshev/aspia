//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_H
#define _ASPIA_HOST__HOST_SESSION_H

#include "base/message_loop/message_loop_thread.h"
#include "base/process/process_watcher.h"
#include "base/synchronization/waitable_timer.h"
#include "host/host_process_connector.h"
#include "host/console_session_watcher.h"
#include "ipc/pipe_channel.h"
#include "network/network_channel_proxy.h"
#include "proto/auth_session.pb.h"

namespace aspia {

class HostSession :
    private ConsoleSessionWatcher::Delegate,
    private MessageLoopThread::Delegate
{
public:
    HostSession(proto::SessionType session_type,
                std::shared_ptr<NetworkChannelProxy> channel_proxy);
    ~HostSession();

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // ConsoleSessionWatcher::Delegate implementation.
    void OnSessionAttached(uint32_t session_id) override;
    void OnSessionDetached() override;

    void OnProcessClose();

    void OnIpcChannelConnect(uint32_t user_data);
    void OnIpcChannelDisconnect();

    void OnSessionAttachTimeout();

    proto::SessionType session_type_;

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    enum class State { STARTING, DETACHED, ATTACHED };

    State state_ = State::DETACHED;

    WaitableTimer timer_;
    ConsoleSessionWatcher session_watcher_;
    Process process_;
    ProcessWatcher process_watcher_;

    std::shared_ptr<NetworkChannelProxy> channel_proxy_;
    HostProcessConnector process_connector_;

    DISALLOW_COPY_AND_ASSIGN(HostSession);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_H
