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

#include "host/desktop_session_manager.h"

#include <QCoreApplication>
#include <QThread>
#include <QVariant>

#include "base/location.h"
#include "base/logging.h"
#include "base/ipc/ipc_channel.h"
#include "host/desktop_session_fake.h"
#include "host/desktop_session_ipc.h"
#include "host/desktop_session_process.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/session_info.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

const std::chrono::minutes kSessionAttachTimeout { 1 };

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopSessionManager::DesktopSessionManager(QObject* parent)
    : QObject(parent),
      session_attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    session_attach_timer_->setSingleShot(true);

    connect(session_attach_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(ERROR) << "Session attach timeout (sid" << session_id_
                   << "timeout" << kSessionAttachTimeout.count() << "min)";
        onErrorOccurred();
    });

    qApp->setProperty("SCREEN_CAPTURE_FPS", defaultCaptureFps());
}

//--------------------------------------------------------------------------------------------------
DesktopSessionManager::~DesktopSessionManager()
{
    LOG(INFO) << "Dtor";

    setState(FROM_HERE, State::STOPPING);
    dettachSession(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
// static
int DesktopSessionManager::defaultCaptureFps()
{
    int default_capture_fps = kDefaultScreenCaptureFps;

    if (qEnvironmentVariableIsSet("ASPIA_DEFAULT_FPS"))
    {
        bool ok = false;
        int default_fps = qEnvironmentVariableIntValue("ASPIA_DEFAULT_FPS", &ok);
        if (ok)
        {
            LOG(INFO) << "Default FPS specified by environment variable";

            if (default_fps < 1 || default_fps > 60)
            {
                LOG(INFO) << "Environment variable contains an incorrect default FPS:" << default_fps;
            }
            else
            {
                default_capture_fps = default_fps;
            }
        }
    }

    return default_capture_fps;
}

//--------------------------------------------------------------------------------------------------
// static
int DesktopSessionManager::minCaptureFps()
{
    int min_capture_fps = kMinScreenCaptureFps;

    if (qEnvironmentVariableIsSet("ASPIA_MIN_FPS"))
    {
        bool ok = false;
        int min_fps = qEnvironmentVariableIntValue("ASPIA_MIN_FPS", &ok);
        if (ok)
        {
            LOG(INFO) << "Minimum FPS specified by environment variable";

            if (min_fps < 1 || min_fps > 60)
            {
                LOG(INFO) << "Environment variable contains an incorrect minimum FPS:" << min_fps;
            }
            else
            {
                min_capture_fps = min_fps;
            }
        }
    }

    return min_capture_fps;
}

//--------------------------------------------------------------------------------------------------
// static
int DesktopSessionManager::maxCaptureFps()
{
    int max_capture_fps = kMaxScreenCaptureFpsHighEnd;

    bool max_fps_from_env = false;
    if (qEnvironmentVariableIsSet("ASPIA_MAX_FPS"))
    {
        bool ok = false;
        int max_fps = qEnvironmentVariableIntValue("ASPIA_MAX_FPS", &ok);
        if (ok)
        {
            LOG(INFO) << "Maximum FPS specified by environment variable";

            if (max_fps < 1 || max_fps > 60)
            {
                LOG(INFO) << "Environment variable contains an incorrect maximum FPS:" << max_fps;
            }
            else
            {
                max_capture_fps = max_fps;
                max_fps_from_env = true;
            }
        }
    }

    if (!max_fps_from_env)
    {
        quint32 threads = QThread::idealThreadCount();
        if (threads <= 2)
        {
            LOG(INFO) << "Low-end CPU detected. Maximum capture FPS:" << kMaxScreenCaptureFpsLowEnd;
            max_capture_fps = kMaxScreenCaptureFpsLowEnd;
        }
    }

    return max_capture_fps;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::attachSession(const base::Location& location, base::SessionId session_id)
{
    if (state_ == State::ATTACHED)
    {
        LOG(INFO) << "Already attached. Session ID:" << session_id << "(from" << location << ")";
        return;
    }

    LOG(INFO) << "Attach session with ID:" << session_id << "current state:" << state_
              << "(from" << location << ")";
    session_id_ = session_id;

    if (state_ == State::STOPPED)
        session_attach_timer_->start(kSessionAttachTimeout);

    setState(FROM_HERE, State::STARTING);

    LOG(INFO) << "Starting desktop session";
    LOG(INFO) << "#####################################################";

#if defined(Q_OS_WINDOWS)
    base::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(ERROR) << "Unable to get session info (sid" << session_id << ")";
        return;
    }

    LOG(INFO) << "# Session info (sid=" << session_id
              << "username=" << session_info.userName()
              << "connect_state=" << session_info.connectState()
              << "win_station=" << session_info.winStationName()
              << "domain=" << session_info.domain()
              << "locked=" << session_info.isUserLocked() << ")";
#endif // defined(Q_OS_WINDOWS)

    QString channel_id = base::IpcServer::createUniqueId();

    LOG(INFO) << "Starting IPC server for desktop session (channel_id" << channel_id << ")";

    ipc_server_ = new base::IpcServer(this);

    connect(ipc_server_, &base::IpcServer::sig_newConnection,
            this, &DesktopSessionManager::onNewIpcConnection);
    connect(ipc_server_, &base::IpcServer::sig_errorOccurred,
            this, &DesktopSessionManager::onErrorOccurred);

    if (!ipc_server_->start(channel_id))
    {
        LOG(ERROR) << "Failed to start IPC server (channel_id" << channel_id << ")";

        onErrorOccurred();
        return;
    }

    LOG(INFO) << "Starting desktop session process";

    std::unique_ptr<DesktopSessionProcess> process = std::make_unique<DesktopSessionProcess>();

    connect(process.get(), &DesktopSessionProcess::sig_stateChanged,
            this, [this, session_id, channel_id](DesktopSessionProcess::State state)
    {
        switch (state)
        {
            case DesktopSessionProcess::State::ERROR_OCURRED:
            {
                LOG(ERROR) << "Failed to create session process (sid" << session_id
                           << "channel_id" << channel_id << ")";
                onErrorOccurred();
            }
            break;

            case DesktopSessionProcess::State::STARTED:
            {
                LOG(INFO) << "Desktop session process created (sid" << session_id
                          << "channel_id" << channel_id << ")";
            }
            break;

            default:
                break;
        }
    });

    process->start(session_id, channel_id);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::dettachSession(const base::Location& location)
{
    if (state_ == State::STOPPED || state_ == State::DETACHED)
    {
        LOG(INFO) << "Session already stopped or dettached (" << state_ << ")";
        return;
    }

    LOG(INFO) << "Dettach session (sid" << session_id_ << "state" << state_ << "from" << location << ")";

    if (state_ != State::STOPPING)
        setState(FROM_HERE, State::DETACHED);

    session_attach_timer_->stop();

    if (session_)
    {
        session_->deleteLater();
        session_ = nullptr;
    }

    LOG(INFO) << "Session process is detached (sid" << session_id_ << ")";

    if (state_ == State::STOPPING)
        return;

    session_attach_timer_->start(kSessionAttachTimeout);

    // The real session process has ended. We create a temporary fake session.
    session_ = new DesktopSessionFake(this);

    startDesktopSession();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::setMouseLock(bool enable)
{
    is_mouse_locked_ = enable;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::setKeyboardLock(bool enable)
{
    is_keyboard_locked_ = enable;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::setPaused(bool enable)
{
    is_paused_ = enable;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::control(proto::internal::DesktopControl::Action action)
{
    if (session_)
        session_->control(action);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::configure(const DesktopSession::Config& config)
{
    if (is_paused_ || !session_)
        return;

    session_->configure(config);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::selectScreen(const proto::desktop::Screen& screen)
{
    if (is_paused_ || !session_)
        return;

    session_->selectScreen(screen);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::captureScreen()
{
    if (is_paused_ || !session_)
        return;

    session_->captureScreen();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::setScreenCaptureFps(int fps)
{
    qApp->setProperty("SCREEN_CAPTURE_FPS", fps);

    if (session_)
        session_->setScreenCaptureFps(fps);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (is_keyboard_locked_ || is_paused_ || !session_)
        return;

    session_->injectKeyEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::injectTextEvent(const proto::desktop::TextEvent& event)
{
    if (is_keyboard_locked_ || is_paused_ || !session_)
        return;

    session_->injectTextEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::injectMouseEvent(const proto::desktop::MouseEvent& event)
{
    if (is_mouse_locked_ || is_paused_ || !session_)
        return;

    session_->injectMouseEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::injectTouchEvent(const proto::desktop::TouchEvent &event)
{
    if (is_mouse_locked_ || is_paused_ || !session_)
        return;

    session_->injectTouchEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (is_paused_ || !session_)
        return;

    session_->injectClipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onNewIpcConnection()
{
    LOG(INFO) << "Session process successfully connected (sid" << session_id_ << ")";

    if (!ipc_server_)
    {
        LOG(ERROR) << "No IPC server instance!";
        return;
    }

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending connections in IPC server";
        return;
    }

    base::IpcChannel* channel = ipc_server_->nextPendingConnection();

    session_attach_timer_->stop();
    ipc_server_->deleteLater();

    session_ = new DesktopSessionIpc(channel, this);

    setState(FROM_HERE, State::ATTACHED);
    startDesktopSession();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::onErrorOccurred()
{
    if (state_ == State::STOPPED || state_ == State::STOPPING)
    {
        LOG(INFO) << "Error skipped (state" << state_ << ")";
        return;
    }

    setState(FROM_HERE, State::STOPPING);
    dettachSession(FROM_HERE);
    setState(FROM_HERE, State::STOPPED);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::setState(const base::Location& location, State state)
{
    LOG(INFO) << "State changed from" << state_ << "to" << state << "(from" << location << ")";
    state_ = state;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionManager::startDesktopSession()
{
    connect(session_, &DesktopSession::sig_desktopSessionStarted,
            this, &DesktopSessionManager::sig_desktopSessionStarted);
    connect(session_, &DesktopSession::sig_desktopSessionStopped,
            this, &DesktopSessionManager::sig_desktopSessionStopped);
    connect(session_, &DesktopSession::sig_screenCaptured,
            this, &DesktopSessionManager::sig_screenCaptured);
    connect(session_, &DesktopSession::sig_screenCaptureError,
            this, &DesktopSessionManager::sig_screenCaptureError);
    connect(session_, &DesktopSession::sig_audioCaptured,
            this, &DesktopSessionManager::sig_audioCaptured);
    connect(session_, &DesktopSession::sig_cursorPositionChanged,
            this, &DesktopSessionManager::sig_cursorPositionChanged);
    connect(session_, &DesktopSession::sig_screenListChanged,
            this, &DesktopSessionManager::sig_screenListChanged);
    connect(session_, &DesktopSession::sig_screenTypeChanged,
            this, &DesktopSessionManager::sig_screenTypeChanged);
    connect(session_, &DesktopSession::sig_clipboardEvent,
            this, &DesktopSessionManager::sig_clipboardEvent);

    session_->setScreenCaptureFps(qApp->property("SCREEN_CAPTURE_FPS").toInt());
    session_->start();
}

} // namespace host
