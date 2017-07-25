//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_console.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_console.h"
#include "host/console_session_launcher.h"
#include "base/version_helpers.h"
#include "base/scoped_privilege.h"
#include "ipc/pipe_channel_proxy.h"

namespace aspia {

static const std::chrono::milliseconds kAttachTimeout{ 30000 };

// static
std::unique_ptr<HostSessionConsole>
HostSessionConsole::CreateForDesktopManage(HostSession::Delegate* delegate)
{
    return std::unique_ptr<HostSessionConsole>(
        new HostSessionConsole(proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE,
                               delegate));
}

// static
std::unique_ptr<HostSessionConsole>
HostSessionConsole::CreateForDesktopView(HostSession::Delegate* delegate)
{
    return std::unique_ptr<HostSessionConsole>(
        new HostSessionConsole(proto::SessionType::SESSION_TYPE_DESKTOP_VIEW,
                               delegate));
}

// static
std::unique_ptr<HostSessionConsole>
HostSessionConsole::CreateForFileTransfer(HostSession::Delegate* delegate)
{
    return std::unique_ptr<HostSessionConsole>(
        new HostSessionConsole(proto::SessionType::SESSION_TYPE_FILE_TRANSFER,
                               delegate));
}

HostSessionConsole::HostSessionConsole(proto::SessionType session_type,
                                       HostSession::Delegate* delegate) :
    HostSession(delegate),
    session_type_(session_type)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

HostSessionConsole::~HostSessionConsole()
{
    ui_thread_.Stop();
}

void HostSessionConsole::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    OnSessionAttached(WTSGetActiveConsoleSessionId());

    // In Windows XP/2003, the console session always has a ID equal to 0.
    // We do not need to watch the session.
    if (IsWindowsVistaOrGreater())
    {
        bool ret = session_watcher_.StartWatching(this);
        DCHECK(ret);
    }
}

void HostSessionConsole::OnAfterThreadRunning()
{
    session_watcher_.StopWatching();
    OnSessionDetached();
    timer_.Stop();

    delegate_->OnSessionTerminate();
}

void HostSessionConsole::OnSessionAttached(uint32_t session_id)
{
    DCHECK(runner_->BelongsToCurrentThread());
    DCHECK(state_ == State::DETACHED);
    DCHECK(!process_.IsValid());
    DCHECK(!ipc_channel_);

    {
        std::lock_guard<std::mutex> lock(ipc_channel_lock_);

        state_ = State::STARTING;

        std::wstring channel_id;

        ipc_channel_ = PipeChannel::CreateServer(channel_id);
        if (ipc_channel_)
        {
            ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

            bool launched;

            switch (session_type_)
            {
                case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
                case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
                    launched = LaunchDesktopSession(session_id, channel_id);
                    break;

                case proto::SessionType::SESSION_TYPE_FILE_TRANSFER:
                    launched = LaunchFileTransferSession(session_id, channel_id);
                    break;

                default:
                    launched = false;
                    break;
            }

            PipeChannel::ConnectHandler connect_handler =
                std::bind(&HostSessionConsole::OnPipeChannelConnect, this, std::placeholders::_1);

            PipeChannel::DisconnectHandler disconnect_handler =
                std::bind(&HostSessionConsole::OnPipeChannelDisconnect, this);

            if (launched && ipc_channel_->Connect(session_type_,
                                                  std::move(connect_handler),
                                                  std::move(disconnect_handler)))
            {
                return;
            }
        }
    }

    runner_->PostQuit();
}

void HostSessionConsole::OnSessionDetached()
{
    DCHECK(runner_->BelongsToCurrentThread());

    if (state_ == State::DETACHED)
        return;

    state_ = State::DETACHED;

    std::lock_guard<std::mutex> lock(ipc_channel_lock_);

    process_watcher_.StopWatching();

    if (session_type_ != proto::SessionType::SESSION_TYPE_FILE_TRANSFER)
    {
        process_.Terminate(0, false);
    }

    process_.Close();

    ipc_channel_.reset();

    // If the new session is not connected within the specified time interval,
    // an error occurred.
    timer_.Start(kAttachTimeout,
                 std::bind(&HostSessionConsole::OnSessionAttachTimeout, this));
}

void HostSessionConsole::OnObjectSignaled(HANDLE object)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionConsole::OnObjectSignaled,
                                    this,
                                    object));
        return;
    }

    DCHECK_EQ(object, process_.Handle());

    switch (state_)
    {
        case State::ATTACHED:
        case State::STARTING:
            OnSessionDetached();
            break;

        case State::DETACHED:
            break;
    }
}

void HostSessionConsole::OnPipeChannelConnect(uint32_t user_data)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &HostSessionConsole::OnPipeChannelConnect, this, user_data));
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
                   << GetLastSystemErrorString();
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
        state_ = State::DETACHED;
        runner_->PostQuit();
        return;
    }

    state_ = State::ATTACHED;

    ipc_channel_proxy_->Receive(
        std::bind(&HostSessionConsole::OnPipeChannelMessage, this, std::placeholders::_1));
}

void HostSessionConsole::OnPipeChannelDisconnect()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionConsole::OnPipeChannelDisconnect, this));
        return;
    }

    switch (state_)
    {
        case State::DETACHED:
            break;

        case State::ATTACHED:
        {
            OnSessionDetached();

            if (session_type_ == proto::SessionType::SESSION_TYPE_FILE_TRANSFER)
            {
                state_ = State::DETACHED;

                timer_.Stop();
                runner_->PostQuit();
            }
        }
        break;

        case State::STARTING:
        {
            DCHECK(!process_.IsValid());

            state_ = State::DETACHED;

            timer_.Stop();
            runner_->PostQuit();
        }
        break;
    }
}

void HostSessionConsole::OnPipeChannelMessage(IOBuffer buffer)
{
    delegate_->OnSessionMessage(std::move(buffer));

    ipc_channel_proxy_->Receive(
        std::bind(&HostSessionConsole::OnPipeChannelMessage, this, std::placeholders::_1));
}

void HostSessionConsole::OnSessionAttachTimeout()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionConsole::OnSessionAttachTimeout, this));
        return;
    }

    switch (state_)
    {
        case State::DETACHED:
        case State::STARTING:
            runner_->PostQuit();
            break;

        case State::ATTACHED:
            LOG(FATAL) << "In the attached state, the timer must me stopped";
            break;
    }
}

void HostSessionConsole::Send(IOBuffer buffer)
{
    std::lock_guard<std::mutex> lock(ipc_channel_lock_);
    ipc_channel_proxy_->Send(std::move(buffer), nullptr);
}

} // namespace aspia
