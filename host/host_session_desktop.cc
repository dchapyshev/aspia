//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_desktop.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_desktop.h"
#include "host/desktop_session_launcher.h"
#include "base/version_helpers.h"
#include "base/scoped_privilege.h"

namespace aspia {

static const std::chrono::milliseconds kAttachTimeout{ 30000 };

// static
std::unique_ptr<HostSessionDesktop> HostSessionDesktop::Create(proto::SessionType session_type,
                                                               HostSession::Delegate* delegate)
{
    switch (session_type)
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
            break;

        default:
            return nullptr;
    }

    return std::unique_ptr<HostSessionDesktop>(new HostSessionDesktop(session_type, delegate));
}

HostSessionDesktop::HostSessionDesktop(proto::SessionType session_type,
                                       HostSession::Delegate* delegate) :
    HostSession(delegate),
    session_type_(session_type)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

HostSessionDesktop::~HostSessionDesktop()
{
    ui_thread_.Stop();
}

void HostSessionDesktop::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    OnSessionAttached(WTSGetActiveConsoleSessionId());

    // In Windows XP, the console session always has a ID equal to 0.
    // We do not need to watch the session.
    if (IsWindowsVistaOrGreater())
    {
        bool ret = session_watcher_.StartWatching(this);
        DCHECK(ret);
    }
}

void HostSessionDesktop::OnAfterThreadRunning()
{
    session_watcher_.StopWatching();
    OnSessionDetached();
    timer_.Stop();
}

void HostSessionDesktop::OnSessionAttached(uint32_t session_id)
{
    DCHECK(runner_->BelongsToCurrentThread());
    DCHECK(state_ == State::Detached);
    DCHECK(!process_.IsValid());
    DCHECK(!ipc_channel_);

    {
        std::lock_guard<std::mutex> lock(ipc_channel_lock_);

        state_ = State::Starting;

        std::wstring input_channel_id;
        std::wstring output_channel_id;

        ipc_channel_ = PipeChannel::CreateServer(input_channel_id,
                                                 output_channel_id);
        if (ipc_channel_)
        {
            if (DesktopSessionLauncher::LaunchSession(session_id,
                                                      input_channel_id,
                                                      output_channel_id))
            {
                if (ipc_channel_->Connect(session_type_, this))
                    return;
            }
        }
    }

    session_watcher_.StopWatching();
    timer_.Stop();

    delegate_->OnSessionTerminate();
}

void HostSessionDesktop::OnSessionDetached()
{
    DCHECK(runner_->BelongsToCurrentThread());

    if (state_ == State::Detached)
        return;

    state_ = State::Detached;

    std::lock_guard<std::mutex> lock(ipc_channel_lock_);

    process_watcher_.StopWatching();

    process_.Terminate(0, false);
    process_.Close();

    ipc_channel_.reset();

    // If the new session is not connected within the specified time interval,
    // an error occurred.
    timer_.Start(kAttachTimeout,
                 std::bind(&HostSessionDesktop::OnSessionAttachTimeout, this));
}

void HostSessionDesktop::OnObjectSignaled(HANDLE object)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionDesktop::OnObjectSignaled,
                                    this,
                                    object));
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

void HostSessionDesktop::OnPipeChannelConnect(uint32_t user_data)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionDesktop::OnPipeChannelConnect,
                                    this,
                                    user_data));
        return;
    }

    // To open a host process and terminate it, additional privileges are required.
    // If the current process is running from the service, then the privilege
    // already exists, if not, then enable it.
    ScopedProcessPrivilege privilege(SE_DEBUG_NAME);

    // The client sends a process ID in user_data.
    process_ = Process::Open(user_data);

    bool ok = process_.IsValid();
    if (!ok)
    {
        LOG(ERROR) << "Unable to open session process: "
                   << GetLastSystemErrorCodeString();
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

void HostSessionDesktop::OnPipeChannelDisconnect()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionDesktop::OnPipeChannelDisconnect, this));
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

void HostSessionDesktop::OnPipeChannelMessage(const IOBuffer& buffer)
{
    delegate_->OnSessionMessage(buffer);
}

void HostSessionDesktop::OnSessionAttachTimeout()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionDesktop::OnSessionAttachTimeout, this));
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

void HostSessionDesktop::Send(const IOBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(ipc_channel_lock_);

    if (!ipc_channel_)
        return;

    ipc_channel_->Send(buffer);
}

} // namespace aspia
