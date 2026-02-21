//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/desktop_manager.h"

#include "base/application.h"
#include "base/logging.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_client.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/session_status.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

const std::chrono::milliseconds kRestartTimeout { 3000 };
const std::chrono::milliseconds kAttachTimeout { 15000 };

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopManager::DesktopManager(QObject* parent)
    : QObject(parent),
      restart_timer_(new QTimer(this)),
      attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";
    instance_ = this;

    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &DesktopManager::onUserSessionEvent);

    restart_timer_->setInterval(kRestartTimeout);
    restart_timer_->setSingleShot(true);

    attach_timer_->setInterval(kAttachTimeout);
    attach_timer_->setSingleShot(true);

    connect(restart_timer_, &QTimer::timeout, this, &DesktopManager::onRestartTimeout);
    connect(attach_timer_, &QTimer::timeout, this, &DesktopManager::onAttachTimeout);
}

//--------------------------------------------------------------------------------------------------
DesktopManager::~DesktopManager()
{
    LOG(INFO) << "Dtor";
    instance_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
DesktopManager* DesktopManager::instance()
{
    return instance_;
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onNewChannel(base::TcpChannel* tcp_channel)
{
    if (!tcp_channel)
    {
        LOG(ERROR) << "Invalid tcp channel pointer";
        return;
    }

    DesktopClient* client = new DesktopClient(tcp_channel, this);
    clients_.append(client);

    connect(this, &DesktopManager::sig_ipcChannelChanged, client, &DesktopClient::onIpcChannelChanged);
    connect(client, &DesktopClient::sig_finished, this, &DesktopManager::onClientFinished);

    client->start(ipc_channel_name_);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::start()
{
    if (process_)
    {
        LOG(ERROR) << "Desktop session process is already started";
        return;
    }

    attach(FROM_HERE, base::activeConsoleSessionId());
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserSessionEvent(quint32 event_type, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "Session event:" << base::sessionStatusToString(event_type)
              << "session id:" << session_id;

    switch (event_type)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (!is_console_)
            {
                LOG(INFO) << "Ignore event for non-console session";
                return;
            }

            attach(FROM_HERE, session_id);
        }
        break;

        case WTS_CONSOLE_DISCONNECT:
        {
            if (!is_console_)
            {
                LOG(INFO) << "Ignore event for non-console session";
                return;
            }

            dettach(FROM_HERE);
        }
        break;

        case WTS_REMOTE_DISCONNECT:
        {
            if (is_console_)
            {
                LOG(INFO) << "Ignore event for console session";
                return;
            }

            dettach(FROM_HERE);
            attach(FROM_HERE, base::activeConsoleSessionId());
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onProcessStateChanged(DesktopProcess::State state)
{
    LOG(INFO) << "Process state changed:" << state << "(" << session_id_ << ")";

    switch (state)
    {
        case DesktopProcess::State::STARTING:
        {
            // Nothing
        }
        break;

        case DesktopProcess::State::STARTED:
        {
            attach_timer_->stop();
            emit sig_ipcChannelChanged(ipc_channel_name_);
        }
        break;

        case DesktopProcess::State::STOPPED:
        {
            // We don't handle process termination in any way. Instead, we wait for session events
            // in onUserSessionEvent slot to restart.
        }
        break;

        case DesktopProcess::State::ERROR_OCURRED:
        {
            // An error occurred while starting the process. Detach the session and start the timer
            // to try to launch it again.
            dettach(FROM_HERE);
            restart_timer_->start();
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onRestartTimeout()
{
    dettach(FROM_HERE);
    attach(FROM_HERE, session_id_);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onAttachTimeout()
{
    LOG(INFO) << "Attach timeout. Restarting after" << kRestartTimeout.count() << "ms";
    restart_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientFinished()
{
    DesktopClient* client = dynamic_cast<DesktopClient*>(sender());
    if (!client)
    {
        LOG(ERROR) << "Unknown sender for finish slot";
        return;
    }

    client->disconnect(this); // Disoconnect all signals.
    client->deleteLater();
    clients_.removeOne(client);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::attach(const base::Location& location, base::SessionId session_id)
{
    if (process_)
    {
        LOG(INFO) << "Session already attached (session_id" << session_id_ << "from" << location << ")";
        return;
    }

    LOG(INFO) << "Attach to session" << session_id << "from" << location;

    session_id_ = session_id;
    is_console_ = session_id == base::activeConsoleSessionId();
    ipc_channel_name_ = base::IpcServer::createUniqueId();

    process_ = new DesktopProcess(this);

    connect(process_, &DesktopProcess::sig_stateChanged, this, &DesktopManager::onProcessStateChanged);

    attach_timer_->start();
    process_->start(session_id, ipc_channel_name_);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::dettach(const base::Location& location)
{
    if (!process_)
    {
        LOG(INFO) << "Session already dettached (session_id" << session_id_ << "from" << location << ")";
        return;
    }

    LOG(INFO) << "Dettach from session" << session_id_ << "from" << location;

    attach_timer_->stop();

    process_->disconnect(this); // Disconnect all signals.
    process_->kill(); // Kill process.
    process_->deleteLater();
    process_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
thread_local DesktopManager* DesktopManager::instance_ = nullptr;

} // namespace host
