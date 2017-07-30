//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session.h"
#include "host/console_session_launcher.h"
#include "base/version_helpers.h"
#include "base/scoped_privilege.h"

namespace aspia {

static const std::chrono::seconds kAttachTimeout{ 30 };

// static
std::unique_ptr<HostSession> HostSession::CreateForDesktopManage(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    return std::unique_ptr<HostSession>(
        new HostSession(proto::SESSION_TYPE_DESKTOP_MANAGE, channel_proxy));
}

// static
std::unique_ptr<HostSession> HostSession::CreateForDesktopView(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    return std::unique_ptr<HostSession>(
        new HostSession(proto::SESSION_TYPE_DESKTOP_VIEW, channel_proxy));
}

// static
std::unique_ptr<HostSession> HostSession::CreateForFileTransfer(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
    return std::unique_ptr<HostSession>(
        new HostSession(proto::SESSION_TYPE_FILE_TRANSFER, channel_proxy));
}

// static
std::unique_ptr<HostSession> HostSession::CreateForPowerManage(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
{
        return std::unique_ptr<HostSession>(
            new HostSession(proto::SESSION_TYPE_POWER_MANAGE, channel_proxy));
}

HostSession::HostSession(
    proto::SessionType session_type,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : session_type_(session_type),
      channel_proxy_(channel_proxy),
      process_connector_(channel_proxy)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

HostSession::~HostSession()
{
    ui_thread_.Stop();
}

void HostSession::OnBeforeThreadRunning()
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

void HostSession::OnAfterThreadRunning()
{
    session_watcher_.StopWatching();
    OnSessionDetached();
    timer_.Stop();
    channel_proxy_->Disconnect();
}

void HostSession::OnSessionAttached(uint32_t session_id)
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
            case proto::SESSION_TYPE_DESKTOP_MANAGE:
            case proto::SESSION_TYPE_DESKTOP_VIEW:
                launched = LaunchDesktopSession(session_id, channel_id);
                break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                launched = LaunchFileTransferSession(session_id, channel_id);
                break;

            case proto::SESSION_TYPE_POWER_MANAGE:
                launched = LaunchPowerManageSession(session_id, channel_id);
                break;

            default:
                launched = false;
                break;
        }

        PipeChannel::DisconnectHandler disconnect_handler =
            std::bind(&HostSession::OnIpcChannelDisconnect, this);

        uint32_t user_data = static_cast<uint32_t>(session_type_);

        if (launched &&
            process_connector_.Accept(user_data,
                                      std::move(disconnect_handler)))
        {
            OnIpcChannelConnect(user_data);
            return;
        }
    }

    runner_->PostQuit();
}

void HostSession::OnSessionDetached()
{
    DCHECK(runner_->BelongsToCurrentThread());

    if (state_ == State::DETACHED)
        return;

    state_ = State::DETACHED;

    process_connector_.Disconnect();
    process_watcher_.StopWatching();

    if (session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE ||
        session_type_ == proto::SESSION_TYPE_DESKTOP_VIEW)
    {
        process_.Terminate(0, false);
    }

    process_.Close();

    // If the new session is not connected within the specified time interval,
    // an error occurred.
    timer_.Start(kAttachTimeout,
                 std::bind(&HostSession::OnSessionAttachTimeout, this));
}

void HostSession::OnProcessClose()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSession::OnProcessClose, this));
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

void HostSession::OnIpcChannelConnect(uint32_t user_data)
{
    DCHECK(runner_->BelongsToCurrentThread());

    // To open a host process and terminate it, additional privileges are
    // required. If the current process is running from the service, then the
    // privilege already exists, if not, then enable it.
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
            process_, std::bind(&HostSession::OnProcessClose, this));

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

void HostSession::OnIpcChannelDisconnect()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSession::OnIpcChannelDisconnect, this));
        return;
    }

    switch (state_)
    {
        case State::DETACHED:
            break;

        case State::ATTACHED:
        {
            OnSessionDetached();

            if (session_type_ == proto::SESSION_TYPE_FILE_TRANSFER ||
                session_type_ == proto::SESSION_TYPE_POWER_MANAGE)
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

void HostSession::OnSessionAttachTimeout()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostSession::OnSessionAttachTimeout, this));
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
