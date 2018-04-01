//
// PROJECT:         Aspia
// FILE:            host/host_session.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_H
#define _ASPIA_HOST__HOST_SESSION_H

#include "base/threading/thread.h"
#include "base/process/process_watcher.h"
#include "base/waitable_timer.h"
#include "host/host_process_connector.h"
#include "host/console_session_watcher.h"
#include "ipc/pipe_channel.h"
#include "network/network_channel_proxy.h"
#include "protocol/auth_session.pb.h"

namespace aspia {

class HostSession :
    private ConsoleSessionWatcher::Delegate,
    private Thread::Delegate
{
public:
    HostSession(proto::auth::SessionType session_type,
                std::shared_ptr<NetworkChannelProxy> channel_proxy);
    ~HostSession();

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // ConsoleSessionWatcher::Delegate implementation.
    void OnSessionAttached(uint32_t session_id) override;
    void OnSessionDetached() override;

    void OnProcessClose();

    void OnIpcChannelConnect(uint32_t user_data);
    void OnIpcChannelDisconnect();

    void OnSessionAttachTimeout();

    proto::auth::SessionType session_type_;

    Thread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    enum class State { STARTING, DETACHED, ATTACHED };

    State state_ = State::DETACHED;

    WaitableTimer timer_;
    ConsoleSessionWatcher session_watcher_;
    Process process_;
    ProcessWatcher process_watcher_;

    std::shared_ptr<NetworkChannelProxy> channel_proxy_;
    HostProcessConnector process_connector_;

    Q_DISABLE_COPY(HostSession)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_H
