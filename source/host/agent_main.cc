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

#include "host/agent_main.h"

#include "version.h"
#include "base/core_application.h"
#include "base/logging.h"
#include "base/ipc/ipc_server.h"
#include "base/threading/asio_event_dispatcher.h"
#include "host/file_agent.h"
#include "host/host_utils.h"
#include "host/terminal_agent.h"
#include "host/workers/audio_worker.h"
#include "host/workers/desktop_ipc_worker.h"
#include "host/workers/input_worker.h"
#include "host/workers/screen_worker.h"

#if defined(Q_OS_WINDOWS)
#include "host/win/portable_host.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_MACOS)
#include "base/mac/login_utils.h"
#include "host/screen_capturer_mac.h"
#endif // defined(Q_OS_MACOS)

namespace {

//--------------------------------------------------------------------------------------------------
int runDesktopAgent(CoreApplication& application)
{
#if defined(Q_OS_MACOS)
    if (!ScreenCapturerMac::waitForDisplays())
    {
        LOG(ERROR) << "No online displays; exiting so launchd can restart the agent";
        return 1;
    }
#endif // defined(Q_OS_MACOS)

    HostUtils::printDebugInfo(
        HostUtils::INCLUDE_VIDEO_ADAPTERS | HostUtils::INCLUDE_WINDOW_STATIONS);

    application.addWorker(std::make_unique<DesktopIpcWorker>());
    application.addWorker(std::make_unique<ScreenWorker>());
    application.addWorker(std::make_unique<InputWorker>());
    application.addWorker(std::make_unique<AudioWorker>());

    DesktopIpcWorker* ipc_worker = CoreApplication::findWorker<DesktopIpcWorker>();
    ScreenWorker* screen_worker = CoreApplication::findWorker<ScreenWorker>();
    InputWorker* input_worker = CoreApplication::findWorker<InputWorker>();
    AudioWorker* audio_worker = CoreApplication::findWorker<AudioWorker>();

    bool portable = false;
#if defined(Q_OS_WINDOWS)
    portable = PortableHost::isActive();
#endif // defined(Q_OS_WINDOWS)

    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_selectScreen, screen_worker, &ScreenWorker::onSelectScreen,
                     Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_keyFrameRequested,
                     screen_worker, &ScreenWorker::onKeyFrameRequested, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_preferredSizeChanged,
                     screen_worker, &ScreenWorker::onSetPreferredSize, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_configure, screen_worker, &ScreenWorker::onConfigure,
                     Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_overflowStateChanged,
                     screen_worker, &ScreenWorker::onOverflowStateChanged, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_bandwidthChanged,
                     screen_worker, &ScreenWorker::onBandwidthChanged, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_stopCapture, screen_worker, &ScreenWorker::onStopCapture,
                     Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_paused, screen_worker, &ScreenWorker::onSetPaused,
                     Qt::QueuedConnection);

    QObject::connect(screen_worker, &ScreenWorker::sig_videoData, ipc_worker, &DesktopIpcWorker::onVideoData,
                     Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_cursorShapeData,
                     ipc_worker, &DesktopIpcWorker::onCursorShapeData, Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_cursorPositionData,
                     ipc_worker, &DesktopIpcWorker::onCursorPositionData, Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_screenListData,
                     ipc_worker, &DesktopIpcWorker::onScreenListData, Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_screenTypeData,
                     ipc_worker, &DesktopIpcWorker::onScreenTypeData, Qt::QueuedConnection);

    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_injectKeyEvent,
                     input_worker, &InputWorker::onInjectKeyEvent, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_injectTextEvent,
                     input_worker, &InputWorker::onInjectTextEvent, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_injectMouseEvent,
                     input_worker, &InputWorker::onInjectMouseEvent, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_injectTouchEvent,
                     input_worker, &InputWorker::onInjectTouchEvent, Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_paused, input_worker, &InputWorker::onSetPaused,
                     Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_mouseLocked, input_worker, &InputWorker::onSetMouseLocked,
                     Qt::QueuedConnection);
    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_keyboardLocked,
                     input_worker, &InputWorker::onSetKeyboardLocked, Qt::QueuedConnection);

    // The portable host must not block the input of the user.
    if (!portable)
    {
        QObject::connect(ipc_worker, &DesktopIpcWorker::sig_blockInput, input_worker, &InputWorker::onSetBlockInput,
                         Qt::QueuedConnection);
    }

    QObject::connect(ipc_worker, &DesktopIpcWorker::sig_audioEnabled, audio_worker, &AudioWorker::onSetEnabled,
                     Qt::QueuedConnection);
    QObject::connect(audio_worker, &AudioWorker::sig_audioData, ipc_worker, &DesktopIpcWorker::onAudioData,
                     Qt::QueuedConnection);

#if defined(Q_OS_MACOS)
    // In the session of a logged-in user this agent is the instance of the application that
    // LaunchServices finds, so it is the one that has to answer the click opening the window.
    // At the login window there is nobody to click.
    if (!LoginUtils::isActive())
        return application.exec(CoreApplication::Loop::APPKIT);
#endif // defined(Q_OS_MACOS)

    return application.exec();
}

} // namespace

//--------------------------------------------------------------------------------------------------
int runAgent(int& argc, char* argv[], const char* agent_type)
{
    const bool desktop = qstrcmp(agent_type, "desktop") == 0;
    const bool file = qstrcmp(agent_type, "file") == 0;
    const bool terminal = qstrcmp(agent_type, "terminal") == 0;

    if (!desktop && !file && !terminal)
    {
        LOG(ERROR) << "Unknown --agent value:" << agent_type;
        return 1;
    }

#if defined(Q_OS_WINDOWS)
    HostUtils::setDpiAwareness();
#endif // defined(Q_OS_WINDOWS)

    // On macOS the desktop agent captures the screen on a Qt worker thread that needs a real CFRunLoop
    // (the capture/display APIs deliver on the run loop). Make Qt back its stock QThread dispatchers
    // with CoreFoundation so those threads get one; other platforms and agents simply ignore the
    // variable.
    qputenv("QT_EVENT_DISPATCHER_CORE_FOUNDATION", "1");

    // The desktop agent is a coordinator: it does no I/O on the main thread (each worker runs its
    // own), so the main thread keeps the default (Qt) event dispatcher. The file and terminal agents
    // do their I/O on the main thread and need the asio dispatcher there.
    if (!desktop)
        CoreApplication::setEventDispatcher(new AsioEventDispatcher());

    CoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);
    CoreApplication application(argc, argv);

    if (desktop)
        return runDesktopAgent(application);

    HostUtils::printDebugInfo();

    QString channel_id = qEnvironmentVariable(IpcServer::kChannelIdEnvVar);
    if (channel_id.isEmpty())
    {
        LOG(ERROR) << "Environment variable" << IpcServer::kChannelIdEnvVar << "is not set";
        return 1;
    }

    if (file)
    {
        FileAgent agent;
        agent.start(channel_id);
        return application.exec();
    }

    TerminalAgent agent;
    agent.start(channel_id);
    return application.exec();
}
