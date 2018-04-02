//
// PROJECT:         Aspia
// FILE:            host/host_session.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session.h"

#include <QDebug>

#include "base/win/scoped_privilege.h"
#include "base/system_error_code.h"
#include "host/host_session_launcher.h"

namespace aspia {

namespace {

constexpr std::chrono::seconds kAttachTimeout{ 30 };

} // namespace

HostSession::HostSession(proto::auth::SessionType session_type,
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
    Q_ASSERT(runner_);

    OnSessionAttached(WTSGetActiveConsoleSessionId());

    bool ret = session_watcher_.StartWatching(this);
    Q_ASSERT(ret);
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
    Q_ASSERT(runner_->BelongsToCurrentThread());
    Q_ASSERT(state_ == State::DETACHED);
    Q_ASSERT(!process_.IsValid());

    state_ = State::STARTING;

    std::wstring channel_id;

    if (process_connector_.StartServer(channel_id))
    {
        bool session_process_launched =
            LaunchSessionProcess(session_type_, session_id, channel_id);

        PipeChannel::DisconnectHandler disconnect_handler =
            std::bind(&HostSession::OnIpcChannelDisconnect, this);

        uint32_t user_data = static_cast<uint32_t>(session_type_);

        if (session_process_launched &&
            process_connector_.Accept(user_data, std::move(disconnect_handler)))
        {
            OnIpcChannelConnect(user_data);
            return;
        }
    }

    runner_->PostQuit();
}

void HostSession::OnSessionDetached()
{
    Q_ASSERT(runner_->BelongsToCurrentThread());

    if (state_ == State::DETACHED)
        return;

    state_ = State::DETACHED;

    process_connector_.Disconnect();
    process_watcher_.StopWatching();

    process_.Terminate(0, false);
    process_.Close();

    if (session_type_ == proto::auth::SESSION_TYPE_DESKTOP_MANAGE ||
        session_type_ == proto::auth::SESSION_TYPE_DESKTOP_VIEW)
    {
        // If the new session is not connected within the specified time interval,
        // an error occurred.
        timer_.Start(kAttachTimeout, std::bind(&HostSession::OnSessionAttachTimeout, this));
    }
    else
    {
        // All other types of sessions do not allow reconnection. Closing the session.
        runner_->PostQuit();
    }
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
    Q_ASSERT(runner_->BelongsToCurrentThread());

    // To open a host process and terminate it, additional privileges are
    // required. If the current process is running from the service, then the
    // privilege already exists, if not, then enable it.
    ScopedProcessPrivilege privilege(SE_DEBUG_NAME);

    // The client sends a process ID in user_data.
    process_ = Process::Open(user_data);

    bool ok = process_.IsValid();
    if (!ok)
    {
        qWarning() << "Unable to open session process: " << lastSystemErrorString();
    }

    if (ok)
    {
        ok = process_watcher_.StartWatching(
            process_, std::bind(&HostSession::OnProcessClose, this));

        if (!ok)
        {
            qWarning("Process watcher not started");
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
            OnSessionDetached();
            break;

        case State::STARTING:
        {
            Q_ASSERT(!process_.IsValid());

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
            qFatal("In the attached state, the timer must me stopped");
            break;
    }
}

} // namespace aspia
