//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_console.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_CONSOLE_H
#define _ASPIA_HOST__HOST_SESSION_CONSOLE_H

#include "base/message_loop/message_loop_thread.h"
#include "base/object_watcher.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_timer.h"
#include "ipc/pipe_channel.h"
#include "host/host_session.h"
#include "host/console_session_watcher.h"
#include "proto/auth_session.pb.h"

namespace aspia {

class HostSessionConsole :
    public HostSession,
    private ConsoleSessionWatcher::Delegate,
    private ObjectWatcher::Delegate,
    private MessageLoopThread::Delegate
{
public:
    ~HostSessionConsole();

    static std::unique_ptr<HostSessionConsole>
        CreateForDesktopManage(HostSession::Delegate* delegate);

    static std::unique_ptr<HostSessionConsole>
        CreateForDesktopView(HostSession::Delegate* delegate);

    static std::unique_ptr<HostSessionConsole>
        CreateForFileTransfer(HostSession::Delegate* delegate);

    // HostSession implementation.
    void Send(IOBuffer buffer) override;

private:
    HostSessionConsole(proto::SessionType session_type,
                       HostSession::Delegate* delegate);

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // ConsoleSessionWatcher::Delegate implementation.
    void OnSessionAttached(uint32_t session_id) override;
    void OnSessionDetached() override;

    // ObjectWatcher::Delegate implementation.
    void OnObjectSignaled(HANDLE object) override;

    void OnPipeChannelConnect(uint32_t user_data);
    void OnPipeChannelDisconnect();
    void OnPipeChannelMessage(IOBuffer buffer);

    void OnSessionAttachTimeout();

    proto::SessionType session_type_;

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    enum class State { STARTING, DETACHED, ATTACHED };

    State state_ = State::DETACHED;

    WaitableTimer timer_;

    ConsoleSessionWatcher session_watcher_;

    Process process_;
    ObjectWatcher process_watcher_;

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;
    std::mutex ipc_channel_lock_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionConsole);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_CONSOLE_H
