//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/desktop_session.h"
#include "host/desktop_session_launcher.h"
#include "base/scoped_privilege.h"

namespace aspia {

static const std::chrono::milliseconds kAttachTimeout{ 30000 };

// static
std::unique_ptr<DesktopSession> DesktopSession::Create(HostSession::Delegate* delegate)
{
    return std::unique_ptr<DesktopSession>(new DesktopSession(delegate));
}

DesktopSession::DesktopSession(HostSession::Delegate* delegate) :
    HostSession(delegate)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

DesktopSession::~DesktopSession()
{
    ui_thread_.Stop();
}

void DesktopSession::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    timer_.Start(kAttachTimeout,
                 std::bind(&DesktopSession::OnSessionAttachTimeout, this));

    OnSessionAttached(WTSGetActiveConsoleSessionId());

    bool ret = session_watcher_.StartWatching(this);
    DCHECK(ret);
}

void DesktopSession::OnAfterThreadRunning()
{
    session_watcher_.StopWatching();
    OnSessionDetached();
    timer_.Stop();
}

void DesktopSession::OnSessionAttached(uint32_t session_id)
{
    DCHECK(runner_->BelongsToCurrentThread());
    DCHECK(state_ == State::Detached);
    DCHECK(!process_.IsValid());
    DCHECK(!ipc_channel_);

    {
        std::unique_lock<std::mutex> lock(ipc_channel_lock_);

        state_ = State::Starting;

        std::wstring input_channel_id;
        std::wstring output_channel_id;

        ipc_channel_ = PipeChannel::CreateServer(&input_channel_id,
                                                 &output_channel_id);
        if (ipc_channel_)
        {
            if (DesktopSessionLauncher::LaunchSession(session_id,
                                                      input_channel_id,
                                                      output_channel_id))
            {
                if (ipc_channel_->Connect(this))
                    return;
            }
        }
    }

    session_watcher_.StopWatching();
    timer_.Stop();

    delegate_->OnSessionTerminate();
}

void DesktopSession::OnSessionDetached()
{
    DCHECK(runner_->BelongsToCurrentThread());

    if (state_ == State::Detached)
        return;

    state_ = State::Detached;

    std::unique_lock<std::mutex> lock(ipc_channel_lock_);

    process_watcher_.StopWatching();

    process_.Terminate(0, false);
    process_.Close();

    ipc_channel_.reset();

    Task::Callback callback =
        std::bind(&DesktopSession::OnSessionAttachTimeout, this);

    // If the new session is not connected within the specified time interval,
    // an error occurred.
    timer_.Start(kAttachTimeout, std::move(callback));
}

void DesktopSession::OnObjectSignaled(HANDLE object)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&DesktopSession::OnObjectSignaled, this, object));
        return;
    }

    DCHECK_EQ(object, process_.Handle());

    switch (state_)
    {
        case State::Attached:
        case State::Starting:
            OnSessionDetached();
            break;

        case State::Detached:
            break;
    }
}

void DesktopSession::OnPipeChannelConnect(ProcessId peer_pid)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&DesktopSession::OnPipeChannelConnect, this, peer_pid));
        return;
    }

    // To open a host process and terminate it, additional privileges are required.
    // If the current process is running from the service, then the privilege
    // already exists, if not, then enable it.
    ScopedProcessPrivilege privilege(SE_DEBUG_NAME);

    process_ = Process::Open(peer_pid);

    bool ok = process_.IsValid();
    if (!ok)
    {
        LOG(ERROR) << "Unable to open session process: " << GetLastError();
    }

    if (ok)
    {
        ok = process_watcher_.StartWatching(process_.Handle(), this);
        if (!ok)
        {
            LOG(ERROR) << "Process watcher not started";
            process_.Close();
        }
    }

    timer_.Stop();

    if (!ok)
    {
        state_ = State::Detached;
        delegate_->OnSessionTerminate();
        return;
    }

    state_ = State::Attached;
}

void DesktopSession::OnPipeChannelDisconnect()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&DesktopSession::OnPipeChannelDisconnect, this));
        return;
    }

    switch (state_)
    {
        case State::Detached:
            break;

        case State::Attached:
            OnSessionDetached();
            break;

        case State::Starting:
        {
            DCHECK(!process_.IsValid());

            state_ = State::Detached;

            timer_.Stop();
            delegate_->OnSessionTerminate();
        }
        break;
    }
}

void DesktopSession::OnPipeChannelMessage(const IOBuffer& buffer)
{
    delegate_->OnSessionMessage(buffer);
}

void DesktopSession::OnSessionAttachTimeout()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&DesktopSession::OnSessionAttachTimeout, this));
        return;
    }

    switch (state_)
    {
        case State::Detached:
        case State::Starting:
            delegate_->OnSessionTerminate();
            break;

        case State::Attached:
            LOG(FATAL) << "In the attached state, the timer must me stopped";
            break;
    }
}

void DesktopSession::Send(const IOBuffer& buffer)
{
    std::unique_lock<std::mutex> lock(ipc_channel_lock_);

    if (!ipc_channel_)
        return;

    ipc_channel_->Send(buffer);
}

} // namespace aspia
