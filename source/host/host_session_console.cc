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

namespace aspia {

static const std::chrono::seconds kAttachTimeout{ 30 };

// static
std::unique_ptr<HostSessionConsole> HostSessionConsole::CreateForDesktopManage(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    return std::unique_ptr<HostSessionConsole>(
        new HostSessionConsole(proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE,
                               channel_proxy));
}

// static
std::unique_ptr<HostSessionConsole> HostSessionConsole::CreateForDesktopView(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    return std::unique_ptr<HostSessionConsole>(
        new HostSessionConsole(proto::SessionType::SESSION_TYPE_DESKTOP_VIEW,
                               channel_proxy));
}

// static
std::unique_ptr<HostSessionConsole> HostSessionConsole::CreateForFileTransfer(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    return std::unique_ptr<HostSessionConsole>(
        new HostSessionConsole(proto::SessionType::SESSION_TYPE_FILE_TRANSFER,
                               channel_proxy));
}

HostSessionConsole::HostSessionConsole(
    proto::SessionType session_type,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : session_type_(session_type),
      channel_proxy_(channel_proxy),
      process_connector_(channel_proxy)
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
    channel_proxy_->Disconnect();
}

void HostSessionConsole::OnSessionAttached(uint32_t session_id)
{
    DCHECK(runner_->BelongsToCurrentThread());
    DCHECK(state_ == State::DETACHED);
    DCHECK(!process_.IsValid());

    state_ = State::STARTING;

    std::wstring channel_id;

    if (process_connector_.StartServer(channel_id))
    {
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

        PipeChannel::DisconnectHandler disconnect_handler =
            std::bind(&HostSessionConsole::OnIpcChannelDisconnect, this);

        uint32_t user_data = static_cast<uint32_t>(session_type_);

        if (launched && process_connector_.Accept(user_data, std::move(disconnect_handler)))
        {
            OnIpcChannelConnect(user_data);
            return;
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

    process_connector_.Disconnect();
    process_watcher_.StopWatching();

    if (session_type_ != proto::SessionType::SESSION_TYPE_FILE_TRANSFER)
    {
        process_.Terminate(0, false);
    }

    process_.Close();

    // If the new session is not connected within the specified time interval,
    // an error occurred.
    timer_.Start(kAttachTimeout,
                 std::bind(&HostSessionConsole::OnSessionAttachTimeout, this));
}

void HostSessionConsole::OnProcessClose()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSessionConsole::OnProcessClose, this));
        return;
    }

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

void HostSessionConsole::OnIpcChannelConnect(uint32_t user_data)
{
    DCHECK(runner_->BelongsToCurrentThread());

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
        ok = process_watcher_.StartWatching(
            process_, std::bind(&HostSessionConsole::OnProcessClose, this));

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
}

void HostSessionConsole::OnIpcChannelDisconnect()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &HostSessionConsole::OnIpcChannelDisconnect, this));
        return;
    }

    switch (state_)
    {
        case State::DETACHED:
            break;

        case State::ATTACHED:
        {
            OnSessionDetached();

            if (session_type_ == proto::SESSION_TYPE_FILE_TRANSFER)
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

void HostSessionConsole::OnSessionAttachTimeout()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &HostSessionConsole::OnSessionAttachTimeout, this));
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

} // namespace aspia
