//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__DESKTOP_SESSION_H
#define _ASPIA_HOST__DESKTOP_SESSION_H

#include "base/message_loop/message_loop_thread.h"
#include "base/object_watcher.h"
#include "base/waitable_timer.h"
#include "base/thread.h"
#include "base/process.h"
#include "ipc/pipe_channel.h"
#include "host/host_session.h"
#include "host/console_session_watcher.h"

namespace aspia {

class DesktopSession :
    public HostSession,
    private ConsoleSessionWatcher::Delegate,
    private ObjectWatcher::Delegate,
    private PipeChannel::Delegate,
    private MessageLoopThread::Delegate
{
public:
    ~DesktopSession();

    static std::unique_ptr<DesktopSession> Create(HostSession::Delegate* delegate);

    void Send(const IOBuffer& buffer) override;

private:
    DesktopSession(HostSession::Delegate* delegate);

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // ConsoleSessionWatcher::Delegate implementation.
    void OnSessionAttached(uint32_t session_id) override;
    void OnSessionDetached() override;

    // ObjectWatcher::Delegate implementation.
    void OnObjectSignaled(HANDLE object) override;

    // PipeChannel::Delegate implementation.
    void OnPipeChannelConnect(ProcessId peer_pid) override;
    void OnPipeChannelDisconnect() override;
    void OnPipeChannelMessage(const IOBuffer& buffer) override;

    void OnSessionAttachTimeout();

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    enum class State { Starting, Detached, Attached };

    State state_ = State::Detached;

    WaitableTimer timer_;

    ConsoleSessionWatcher session_watcher_;

    Process process_;
    ObjectWatcher process_watcher_;

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::mutex ipc_channel_lock_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSession);
};

} // namespace aspia

#endif // _ASPIA_HOST__DESKTOP_SESSION_H
