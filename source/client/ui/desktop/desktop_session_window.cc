//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/desktop/desktop_session_window.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QResizeEvent>
#include <QPropertyAnimation>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QWindow>

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/desktop/frame_qimage.h"
#include "base/desktop/mouse_cursor.h"
#include "client/client_desktop.h"
#include "client/ui/desktop/desktop_config_dialog.h"
#include "client/ui/desktop/desktop_settings.h"
#include "client/ui/desktop/desktop_toolbar.h"
#include "client/ui/file_transfer/file_transfer_session_window.h"
#include "client/ui/sys_info/system_info_session_window.h"
#include "client/ui/text_chat/text_chat_session_window.h"
#include "client/ui/port_forwarding/port_forwarding_session_window.h"
#include "client/ui/desktop/statistics_dialog.h"
#include "client/ui/desktop/task_manager_window.h"
#include "common/desktop_session_constants.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/windows_version.h"
#endif // defined(Q_OS_WINDOWS)

namespace client {

namespace {

//--------------------------------------------------------------------------------------------------
QSize scaledSize(const QSize& source_size, int scale)
{
    if (scale == -1)
        return source_size;

    int width = static_cast<int>(static_cast<double>(source_size.width() * scale) / 100.0);
    int height = static_cast<int>(static_cast<double>(source_size.height() * scale) / 100.0);

    return QSize(width, height);
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopSessionWindow::DesktopSessionWindow(proto::peer::SessionType session_type,
                                           const proto::desktop::Config& desktop_config,
                                           QWidget* parent)
    : SessionWindow(nullptr, parent),
      session_type_(session_type),
      desktop_config_(desktop_config)
{
    LOG(INFO) << "Ctor";

    setMinimumSize(400, 300);

    desktop_ = new DesktopWidget(this);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setAlignment(Qt::AlignCenter);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setWidget(desktop_);
    scroll_area_->viewport()->setStyleSheet("background-color: rgb(25, 25, 25);");

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(scroll_area_);

    toolbar_ = new DesktopToolBar(session_type_, scroll_area_);
    toolbar_->installEventFilter(this);

    resize_timer_ = new QTimer(this);
    connect(resize_timer_, &QTimer::timeout, this, &DesktopSessionWindow::onResizeTimer);

    scroll_timer_ = new QTimer(this);
    connect(scroll_timer_, &QTimer::timeout, this, &DesktopSessionWindow::onScrollTimer);

    desktop_->enableKeyCombinations(toolbar_->sendKeyCombinations());
    desktop_->enableRemoteCursorPosition(desktop_config_.flags() & proto::desktop::CURSOR_POSITION);

    connect(toolbar_, &DesktopToolBar::sig_keyCombination, desktop_, &DesktopWidget::executeKeyCombination);
    connect(toolbar_, &DesktopToolBar::sig_settingsButton, this, &DesktopSessionWindow::changeSettings);
    connect(toolbar_, &DesktopToolBar::sig_switchToAutosize, this, &DesktopSessionWindow::autosizeWindow);
    connect(toolbar_, &DesktopToolBar::sig_takeScreenshot, this, &DesktopSessionWindow::takeScreenshot);
    connect(toolbar_, &DesktopToolBar::sig_scaleChanged, this, &DesktopSessionWindow::scaleDesktop);
    connect(toolbar_, &DesktopToolBar::sig_minimizeSession, this, [this]()
    {
        LOG(INFO) << "Minimize from full screen";
        is_minimized_from_full_screen_ = true;
        showMinimized();
    });
    connect(toolbar_, &DesktopToolBar::sig_closeSession, this, &DesktopSessionWindow::close);
    connect(toolbar_, &DesktopToolBar::sig_showHidePanel, this, &DesktopSessionWindow::onShowHidePanel);

    enable_video_pause_ = toolbar_->isVideoPauseEnabled();
    connect(toolbar_, &DesktopToolBar::sig_videoPauseChanged, this, [this](bool enable)
    {
        enable_video_pause_ = enable;
    });

    enable_audio_pause_ = toolbar_->isAudioPauseEnabled();
    connect(toolbar_, &DesktopToolBar::sig_audioPauseChanged, this, [this](bool enable)
    {
        enable_audio_pause_ = enable;
    });

    connect(toolbar_, &DesktopToolBar::sig_screenSelected,
            this, &DesktopSessionWindow::sig_screenSelected);

    connect(toolbar_, &DesktopToolBar::sig_powerControl,
            this, [this](proto::desktop::PowerControl::Action action, bool wait)
    {
        switch (action)
        {
            case proto::desktop::PowerControl::ACTION_REBOOT:
            case proto::desktop::PowerControl::ACTION_REBOOT_SAFE_MODE:
                sessionState()->setAutoReconnect(wait);
                break;

            default:
                break;
        }

        emit sig_powerControl(action);
    });

    connect(toolbar_, &DesktopToolBar::sig_startRemoteUpdate,
            this, &DesktopSessionWindow::sig_remoteUpdate);

    connect(toolbar_, &DesktopToolBar::sig_startSystemInfo, this, [this]()
    {
        if (!system_info_)
        {
            system_info_ = new SystemInfoSessionWindow(sessionState());
            system_info_->setAttribute(Qt::WA_DeleteOnClose);

            connect(system_info_, &SystemInfoSessionWindow::sig_systemInfoRequired,
                    this, &DesktopSessionWindow::sig_systemInfoRequested);
        }

        system_info_->onShowWindow();
    });

    connect(toolbar_, &DesktopToolBar::sig_startTaskManager, this, [this]()
    {
        if (!task_manager_)
        {
            task_manager_ = new TaskManagerWindow();
            task_manager_->setAttribute(Qt::WA_DeleteOnClose);

            connect(task_manager_, &TaskManagerWindow::sig_sendMessage,
                    this, &DesktopSessionWindow::sig_taskManager);
        }

        task_manager_->show();
        task_manager_->activateWindow();
    });

    connect(toolbar_, &DesktopToolBar::sig_startStatistics,
            this, &DesktopSessionWindow::sig_metricsRequested);
    connect(toolbar_, &DesktopToolBar::sig_pasteAsKeystrokes,
            this, &DesktopSessionWindow::onPasteKeystrokes);
    connect(toolbar_, &DesktopToolBar::sig_switchToFullscreen, this, [this](bool fullscreen)
    {
        if (fullscreen)
        {
            is_maximized_ = isMaximized();
            showFullScreen();
        }
        else
        {
            if (is_maximized_)
                showMaximized();
            else
                showNormal();
        }
    });

    connect(toolbar_, &DesktopToolBar::sig_keyCombinationsChanged,
            desktop_, &DesktopWidget::enableKeyCombinations);

    desktop_->installEventFilter(this);
    scroll_area_->viewport()->installEventFilter(this);

    connect(toolbar_, &DesktopToolBar::sig_startSession,
            this, [this](proto::peer::SessionType session_type)
    {
        Config session_config = sessionState()->config();
        session_config.session_type = session_type;

        SessionWindow* session_window = nullptr;

        switch (session_config.session_type)
        {
            case proto::peer::SESSION_TYPE_FILE_TRANSFER:
                session_window = new FileTransferSessionWindow();
                break;

            case proto::peer::SESSION_TYPE_TEXT_CHAT:
                session_window = new TextChatSessionWindow();
                break;

            case proto::peer::SESSION_TYPE_PORT_FORWARDING:
                LOG(ERROR) << "Fix me! Use real config";
                session_window = new PortForwardingSessionWindow(proto::port_forwarding::Config());
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!session_window)
        {
            LOG(ERROR) << "Session window not created";
            return;
        }

        session_window->setAttribute(Qt::WA_DeleteOnClose);
        if (!session_window->connectToHost(session_config))
        {
            LOG(ERROR) << "Unable to connect to host";
            session_window->close();
        }
    });

    connect(toolbar_, &DesktopToolBar::sig_recordingStateChanged, this, [this](bool enable)
    {
        QString file_path;

        if (enable)
        {
            DesktopSettings settings;
            file_path = settings.recordingPath();
        }

        emit sig_videoRecording(enable, file_path);
    });

    connect(desktop_, &DesktopWidget::sig_mouseEvent, this, &DesktopSessionWindow::onMouseEvent);
    connect(desktop_, &DesktopWidget::sig_keyEvent, this, &DesktopSessionWindow::sig_keyEvent);

    desktop_->setFocus();
}

//--------------------------------------------------------------------------------------------------
DesktopSessionWindow::~DesktopSessionWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* DesktopSessionWindow::createClient()
{
    LOG(INFO) << "Create client";

    ClientDesktop* client = new ClientDesktop();

    connect(client, &ClientDesktop::sig_showSessionWindow, this, &DesktopSessionWindow::onShowWindow,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_configRequired, this, &DesktopSessionWindow::onConfigRequired,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_capabilities, this, &DesktopSessionWindow::onCapabilitiesChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_screenListChanged, this, &DesktopSessionWindow::onScreenListChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_screenTypeChanged, this, &DesktopSessionWindow::onScreenTypeChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_cursorPositionChanged, this, &DesktopSessionWindow::onCursorPositionChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_systemInfo, this, &DesktopSessionWindow::onSystemInfoChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_taskManager, this, &DesktopSessionWindow::onTaskManagerChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_metrics, this, &DesktopSessionWindow::onMetricsChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_frameError, this, &DesktopSessionWindow::onFrameError,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_frameChanged, this, &DesktopSessionWindow::onFrameChanged,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_drawFrame, this, &DesktopSessionWindow::onDrawFrame,
            Qt::QueuedConnection);
    connect(client, &ClientDesktop::sig_mouseCursorChanged, this, &DesktopSessionWindow::onMouseCursorChanged,
            Qt::QueuedConnection);

    connect(this, &DesktopSessionWindow::sig_desktopConfigChanged, client, &ClientDesktop::setDesktopConfig,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_screenSelected, client, &ClientDesktop::setCurrentScreen,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_preferredSizeChanged, client, &ClientDesktop::setPreferredSize,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_videoPaused, client, &ClientDesktop::setVideoPause,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_audioPaused, client, &ClientDesktop::setAudioPause,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_videoRecording, client, &ClientDesktop::setVideoRecording,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_keyEvent, client, &ClientDesktop::onKeyEvent,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_textEvent, client, &ClientDesktop::onTextEvent,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_mouseEvent, client, &ClientDesktop::onMouseEvent,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_powerControl, client, &ClientDesktop::onPowerControl,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_remoteUpdate, client, &ClientDesktop::onRemoteUpdate,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_systemInfoRequested, client, &ClientDesktop::onSystemInfoRequest,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_taskManager, client, &ClientDesktop::onTaskManager,
            Qt::QueuedConnection);
    connect(this, &DesktopSessionWindow::sig_metricsRequested, client, &ClientDesktop::onMetricsRequest,
            Qt::QueuedConnection);

    client->setDesktopConfig(desktop_config_);

    return client;
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onShowWindow()
{
    LOG(INFO) << "Show window";

    showNormal();
    activateWindow();

    toolbar_->enableTextChat(true);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onConfigRequired()
{
    LOG(INFO) << "Config required";

    if (!(video_encodings_ & common::kSupportedVideoEncodings))
    {
        LOG(INFO) << "No supported video encodings";
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("There are no supported video encodings."),
                             QMessageBox::Ok);
        close();
    }
    else
    {
        LOG(INFO) << "Current video encoding not supported by host";
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("The current video encoding is not supported by the host. "
                                "Please specify a different video encoding."),
                             QMessageBox::Ok);

        DesktopConfigDialog* dialog = new DesktopConfigDialog(
            session_type_, desktop_config_, video_encodings_, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(dialog, &DesktopConfigDialog::sig_configChanged,
                this, &DesktopSessionWindow::onConfigChanged);
        connect(dialog, &DesktopConfigDialog::rejected, this, &DesktopSessionWindow::close);

        dialog->show();
        dialog->activateWindow();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onCapabilitiesChanged(const proto::desktop::Capabilities& capabilities)
{
    video_encodings_ = capabilities.video_encodings();

    // The list of extensions is passed as a string. Extensions are separated by a semicolon.
    QStringList extensions =
        QString::fromStdString(capabilities.extensions()).split(';', Qt::SkipEmptyParts);

    // By default, remote update is disabled.
    toolbar_->enableRemoteUpdate(false);

    if (extensions.contains(common::kRemoteUpdateExtension))
    {
        if (base::kCurrentVersion > sessionState()->hostVersion())
            toolbar_->enableRemoteUpdate(true);
    }

    toolbar_->enablePowerControl(extensions.contains(common::kPowerControlExtension));
    toolbar_->enableScreenSelect(extensions.contains(common::kSelectScreenExtension));
    toolbar_->enableSystemInfo(extensions.contains(common::kSystemInfoExtension));
    toolbar_->enableTaskManager(extensions.contains(common::kTaskManagerExtension));
    toolbar_->enableVideoPauseFeature(extensions.contains(common::kVideoPauseExtension));
    toolbar_->enableAudioPauseFeature(extensions.contains(common::kAudioPauseExtension));
    toolbar_->enableCtrlAltDelFeature(capabilities.os_type() == proto::desktop::Capabilities::OS_TYPE_WINDOWS);

    for (int i = 0; i < capabilities.flag_size(); ++i)
    {
        const proto::desktop::Capabilities::Flag& flag = capabilities.flag(i);
        const std::string& name = flag.name();
        bool value = flag.value();

        if (name == common::kFlagDisablePasteAsKeystrokes)
        {
            toolbar_->enablePasteAsKeystrokesFeature(!value);
        }
        else if (name == common::kFlagDisableAudio)
        {
            disable_feature_audio_ = value;
        }
        else if (name == common::kFlagDisableClipboard)
        {
            disable_feature_clipboard_ = value;
        }
        else if (name == common::kFlagDisableCursorShape)
        {
            disable_feature_cursor_shape_ = value;
        }
        else if (name == common::kFlagDisableCursorPosition)
        {
            disable_feature_cursor_position_ = value;
        }
        else if (name == common::kFlagDisableDesktopEffects)
        {
            disable_feature_desktop_effects_ = value;
        }
        else if (name == common::kFlagDisableDesktopWallpaper)
        {
            disable_feature_desktop_wallpaper_ = value;
        }
        else if (name == common::kFlagDisableFontSmoothing)
        {
            disable_feature_font_smoothing_ = value;
        }
        else if (name == common::kFlagDisableClearClipboard)
        {
            disable_feature_clear_clipboard_ = value;
        }
        else if (name == common::kFlagDisableLockAtDisconnect)
        {
            disable_feature_lock_at_disconnect_ = value;
        }
        else if (name == common::kFlagDisableBlockInput)
        {
            disable_feature_block_input_ = value;
        }
        else
        {
            LOG(ERROR) << "Unknown flag" << name << "with value" << value;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onScreenListChanged(const proto::desktop::ScreenList& screen_list)
{
    toolbar_->setScreenList(screen_list);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onScreenTypeChanged(const proto::desktop::ScreenType& screen_type)
{
    toolbar_->setScreenType(screen_type);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onCursorPositionChanged(const proto::desktop::CursorPosition& cursor_position)
{
    base::Frame* frame = desktop_->desktopFrame();
    if (!frame)
        return;

    const QSize& frame_size = frame->size();

    int pos_x = static_cast<int>(
        static_cast<double>(desktop_->width() * cursor_position.x()) /
        static_cast<double>(frame_size.width()));
    int pos_y = static_cast<int>(
        static_cast<double>(desktop_->height() * cursor_position.y()) /
        static_cast<double>(frame_size.height()));

    desktop_->setCursorPosition(QPoint(pos_x, pos_y));
    desktop_->update();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onSystemInfoChanged(const proto::system_info::SystemInfo& system_info)
{
    if (system_info_)
        system_info_->onSystemInfoChanged(system_info);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onTaskManagerChanged(const proto::task_manager::HostToClient& message)
{
    if (task_manager_)
        task_manager_->readMessage(message);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onMetricsChanged(const client::ClientDesktop::Metrics& metrics)
{
    if (!statistics_dialog_)
    {
        LOG(INFO) << "Statistics dialog not created yet";

        statistics_dialog_ = new StatisticsDialog(this);
        statistics_dialog_->setAttribute(Qt::WA_DeleteOnClose);

        connect(statistics_dialog_, &StatisticsDialog::sig_metricsRequired,
                this, &DesktopSessionWindow::sig_metricsRequested);

        statistics_dialog_->show();
        statistics_dialog_->activateWindow();
    }

    statistics_dialog_->setMetrics(metrics);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onFrameError(proto::desktop::VideoErrorCode error_code)
{
    desktop_->setDesktopFrameError(error_code);
    desktop_->update();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onFrameChanged(
    const QSize& screen_size, std::shared_ptr<base::Frame> frame)
{
    screen_size_ = screen_size;
    LOG(INFO) << "Screen size changed:" << screen_size_;

    bool has_old_frame = desktop_->desktopFrame() != nullptr;

    desktop_->setDesktopFrame(frame);
    scaleDesktop();

    if (!has_old_frame)
    {
        LOG(INFO) << "Resize window (first frame)";
        autosizeWindow();

        // If the parameters indicate that it is necessary to record the connection session, then we
        // start recording.
        DesktopSettings settings;
        if (settings.recordSessions())
        {
            LOG(INFO) << "Auto-recording enabled";
            toolbar_->startRecording(true);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onDrawFrame()
{
    desktop_->drawDesktopFrame();
    toolbar_->update();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onMouseCursorChanged(std::shared_ptr<base::MouseCursor> mouse_cursor)
{
    QPoint local_dpi(base::MouseCursor::kDefaultDpiX, base::MouseCursor::kDefaultDpiY);
    QPoint remote_dpi = mouse_cursor->constDpi();

    QWidget* current_window = window();
    if (current_window)
    {
        QScreen* current_screen = current_window->screen();
        if (current_screen)
        {
            local_dpi.setX(static_cast<qint32>(current_screen->logicalDotsPerInchX()));
            local_dpi.setY(static_cast<qint32>(current_screen->logicalDotsPerInchY()));
        }
    }

    int width = mouse_cursor->width();
    int height = mouse_cursor->height();
    int hotspot_x = mouse_cursor->hotSpotX();
    int hotspot_y = mouse_cursor->hotSpotY();

    QImage image(reinterpret_cast<const uchar*>(mouse_cursor->constImage().data()), width, height,
                 mouse_cursor->stride(), QImage::Format::Format_ARGB32);

    if (local_dpi != remote_dpi)
    {
        double scale_factor_x =
            static_cast<double>(local_dpi.x()) / static_cast<double>(remote_dpi.x());
        double scale_factor_y =
            static_cast<double>(local_dpi.y()) / static_cast<double>(remote_dpi.y());

        width = std::max(static_cast<int>(static_cast<double>(width) * scale_factor_x), 1);
        height = std::max(static_cast<int>(static_cast<double>(height) * scale_factor_y), 1);
        hotspot_x = static_cast<int>(static_cast<double>(hotspot_x) * scale_factor_x);
        hotspot_y = static_cast<int>(static_cast<double>(hotspot_y) * scale_factor_y);

        QImage scaled_image =
            image.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        desktop_->setCursorShape(QPixmap::fromImage(std::move(scaled_image)),
                                 QPoint(hotspot_x, hotspot_y));
    }
    else
    {
        desktop_->setCursorShape(QPixmap::fromImage(std::move(image)),
                                 QPoint(hotspot_x, hotspot_y));
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onInternalReset()
{
    LOG(INFO) << "Internal reset";

    if (system_info_)
    {
        LOG(INFO) << "Close System Info window";
        system_info_->close();
    }

    if (task_manager_)
    {
        LOG(INFO) << "Close Task Manager window";
        task_manager_->close();
    }

    if (statistics_dialog_)
    {
        LOG(INFO) << "Close Statistics window";
        statistics_dialog_->close();
    }

    video_encodings_ = 0;

    if (resize_timer_)
        resize_timer_->stop();
    screen_size_ = QSize();
    if (scroll_timer_)
        scroll_timer_->stop();
    scroll_delta_ = QPoint();

    enable_video_pause_ = true;
    video_pause_last_ = false;
    enable_audio_pause_ = true;
    audio_pause_last_ = false;

    disable_feature_audio_ = false;
    disable_feature_clipboard_ = false;
    disable_feature_cursor_shape_ = false;
    disable_feature_cursor_position_ = false;
    disable_feature_desktop_effects_ = false;
    disable_feature_desktop_wallpaper_ = false;
    disable_feature_font_smoothing_ = false;
    disable_feature_clear_clipboard_ = false;
    disable_feature_lock_at_disconnect_ = false;
    disable_feature_block_input_ = false;

    wheel_angle_ = QPoint();

    desktop_->setDesktopFrame(nullptr);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::resizeEvent(QResizeEvent* event)
{
    int panel_width = toolbar_->width();
    int window_width = width();

    QPoint new_pos;
    new_pos.setX(((window_width * panel_pos_x_) / 100) - (panel_width / 2));
    new_pos.setY(0);

    if (new_pos.x() < 0)
        new_pos.setX(0);
    else if (new_pos.x() > (window_width - panel_width))
        new_pos.setX(window_width - panel_width);

    toolbar_->move(new_pos);
    scaleDesktop();

    QWidget::resizeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::leaveEvent(QEvent* event)
{
    if (scroll_timer_->isActive())
        scroll_timer_->stop();

    QWidget::leaveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        bool is_minimized = isMinimized();

        LOG(INFO) << "Window minimized:" << is_minimized;

        if (is_minimized)
        {
            if (enable_video_pause_)
            {
                emit sig_videoPaused(true);
                video_pause_last_ = true;
            }

            if (enable_audio_pause_)
            {
                emit sig_audioPaused(true);
                audio_pause_last_ = true;
            }

            desktop_->userLeftFromWindow();
        }
        else
        {
            if (enable_video_pause_ || video_pause_last_)
            {
                if (video_pause_last_)
                {
                    emit sig_videoPaused(false);
                    video_pause_last_ = false;
                }
            }

            if (enable_audio_pause_ || audio_pause_last_)
            {
                if (audio_pause_last_)
                {
                    emit sig_audioPaused(false);
                    audio_pause_last_ = false;
                }
            }
        }
    }

    QWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::showEvent(QShowEvent* event)
{
    if (is_minimized_from_full_screen_)
    {
        LOG(INFO) << "Restore to full screen";
        is_minimized_from_full_screen_ = false;

#if defined(Q_OS_WINDOWS)
        if (base::windowsVersion() >= base::VERSION_WIN11)
        {
            // In Windows 11, when you maximize a minimized window from full screen, the window does
            // not return to full screen. We force the window to full screen.
            // However, in versions of Windows less than 11, this breaks the window's minimization,
            // therefore we use this piece of code only for Windows 11.
            showFullScreen();
        }
#endif // defined(Q_OS_WINDOWS)
    }

    QWidget::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::focusOutEvent(QFocusEvent* event)
{
    LOG(INFO) << "Focus out event";
    desktop_->userLeftFromWindow();
    QWidget::focusOutEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event";

    if (system_info_)
    {
        LOG(INFO) << "Close System Info window";
        system_info_->close();
    }

    if (task_manager_)
    {
        LOG(INFO) << "Close Task Manager window";
        task_manager_->close();
    }

    SessionWindow::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSessionWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == desktop_)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_Tab)
            {
                desktop_->doKeyEvent(key_event);
                return true;
            }
        }

        return false;
    }
    else if (object == scroll_area_->viewport())
    {
        if (event->type() == QEvent::Wheel)
        {
            QWheelEvent* wheel_event = static_cast<QWheelEvent*>(event);

            // High-resolution mice or touchpads may generate small mouse wheel angles (eg 2
            // degrees). We accumulate the rotation angle until it becomes 120 degrees.
            wheel_angle_ += wheel_event->angleDelta();

            if (wheel_angle_.y() >= QWheelEvent::DefaultDeltasPerStep ||
                wheel_angle_.y() <= -QWheelEvent::DefaultDeltasPerStep)
            {
                QPoint pos = desktop_->mapFromGlobal(wheel_event->globalPosition().toPoint());

                desktop_->doMouseEvent(wheel_event->type(),
                                       wheel_event->buttons(),
                                       pos,
                                       wheel_angle_);
                wheel_angle_ = QPoint(0, 0);
            }
            return true;
        }
    }
    else if (object == toolbar_)
    {
        if (event->type() == QEvent::Resize)
        {
            int panel_width = toolbar_->width();
            int window_width = width();

            QPoint new_pos;
            new_pos.setX(((window_width * panel_pos_x_) / 100) - (panel_width / 2));
            new_pos.setY(0);

            if (new_pos.x() < 0)
                new_pos.setX(0);
            else if (new_pos.x() > (window_width - panel_width))
                new_pos.setX(window_width - panel_width);

            toolbar_->move(new_pos);
        }
        else if (event->type() == QEvent::Leave)
        {
            desktop_->setFocus();
        }
        else if (!toolbar_->isPanelHidden())
        {
            if (event->type() == QEvent::MouseButtonPress)
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->button() == Qt::RightButton)
                {
                    start_panel_pos_.emplace(mouse_event->pos());
                    return true;
                }
            }
            else if (event->type() == QEvent::MouseButtonRelease)
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->button() == Qt::RightButton)
                {
                    start_panel_pos_.reset();
                    return true;
                }
            }
            else if (event->type() == QEvent::MouseMove)
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if ((mouse_event->buttons() & Qt::RightButton) && start_panel_pos_.has_value())
                {
                    QPoint diff = mouse_event->pos() - *start_panel_pos_;
                    QPoint new_pos = QPoint(toolbar_->pos().x() + diff.x(), 0);
                    int panel_width = toolbar_->width();
                    int window_width = width();

                    if (new_pos.x() < 0)
                        new_pos.setX(0);
                    else if (new_pos.x() > (window_width - panel_width))
                        new_pos.setX(window_width - panel_width);

                    panel_pos_x_ = ((new_pos.x() + (panel_width / 2)) * 100) / window_width;
                    toolbar_->move(new_pos);
                    return true;
                }
            }
        }
    }

    return QWidget::eventFilter(object, event);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onMouseEvent(const proto::desktop::MouseEvent& event)
{
    QPoint pos(event.x(), event.y());

    if (toolbar_->autoScrolling() && toolbar_->scale() != -1)
    {
        QPoint cursor = desktop_->mapTo(scroll_area_, pos);
        QRect client_area = scroll_area_->rect();

        QScrollBar* hscrollbar = scroll_area_->horizontalScrollBar();
        QScrollBar* vscrollbar = scroll_area_->verticalScrollBar();

        if (!hscrollbar->isHidden())
            client_area.setHeight(client_area.height() - hscrollbar->height());

        if (!vscrollbar->isHidden())
            client_area.setWidth(client_area.width() - vscrollbar->width());

        scroll_delta_.setX(0);
        scroll_delta_.setY(0);

        static const int kThresholdDistance = 80;
        static const int kStep = 10;

        if (client_area.width() < desktop_->width())
        {
            if (cursor.x() > client_area.width() - kThresholdDistance)
                scroll_delta_.setX(kStep);
            else if (cursor.x() < kThresholdDistance)
                scroll_delta_.setX(-kStep);
        }

        if (client_area.height() < desktop_->height())
        {
            if (cursor.y() > client_area.height() - kThresholdDistance)
                scroll_delta_.setY(kStep);
            else if (cursor.y() < kThresholdDistance)
                scroll_delta_.setY(-kStep);
        }

        if (!scroll_delta_.isNull())
        {
            if (!scroll_timer_->isActive())
                scroll_timer_->start(std::chrono::milliseconds(15));
        }
        else if (scroll_timer_->isActive())
        {
            scroll_timer_->stop();
        }
    }
    else if (scroll_timer_->isActive())
    {
        scroll_timer_->stop();
    }

    base::Frame* current_frame = desktop_->desktopFrame();
    if (current_frame)
    {
        const QSize& source_size = current_frame->size();
        QSize scaled_size = desktop_->size();

        double scale_x = (scaled_size.width() * 100) / static_cast<double>(source_size.width());
        double scale_y = (scaled_size.height() * 100) / static_cast<double>(source_size.height());
        double scale = std::min(scale_x, scale_y);

        proto::desktop::MouseEvent out_event;

        out_event.set_mask(event.mask());
        out_event.set_x(static_cast<int>(static_cast<double>(pos.x() * 100) / scale));
        out_event.set_y(static_cast<int>(static_cast<double>(pos.y() * 100) / scale));

        emit sig_mouseEvent(out_event);
    }

    // In MacOS event Leave does not always come to the widget when the mouse leaves its area.
    if (!toolbar_->isPanelHidden() && !toolbar_->isPanelPinned())
    {
        if (!toolbar_->rect().contains(pos))
            QApplication::postEvent(toolbar_, new QEvent(QEvent::Leave));
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::changeSettings()
{
    LOG(INFO) << "Create desktop config dialog";

    DesktopConfigDialog* dialog = new DesktopConfigDialog(
        session_type_, desktop_config_, video_encodings_, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    dialog->enableAudioFeature(!disable_feature_audio_);
    dialog->enableClipboardFeature(!disable_feature_clipboard_);
    dialog->enableCursorShapeFeature(!disable_feature_cursor_shape_);
    dialog->enableCursorPositionFeature(!disable_feature_cursor_position_);
    dialog->enableDesktopEffectsFeature(!disable_feature_desktop_effects_);
    dialog->enableDesktopWallpaperFeature(!disable_feature_desktop_wallpaper_);
    dialog->enableFontSmoothingFeature(!disable_feature_font_smoothing_);
    dialog->enableClearClipboardFeature(!disable_feature_clear_clipboard_);
    dialog->enableLockAtDisconnectFeature(!disable_feature_lock_at_disconnect_);
    dialog->enableBlockInputFeature(!disable_feature_block_input_);

    connect(dialog, &DesktopConfigDialog::sig_configChanged, this, &DesktopSessionWindow::onConfigChanged);

    dialog->show();
    dialog->activateWindow();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onConfigChanged(const proto::desktop::Config& desktop_config)
{
    LOG(INFO) << "Desktop config changed";

    desktop_config_ = desktop_config;

    emit sig_desktopConfigChanged(desktop_config);

    desktop_->enableRemoteCursorPosition(desktop_config_.flags() & proto::desktop::CURSOR_POSITION);
    if (!(desktop_config_.flags() & proto::desktop::ENABLE_CURSOR_SHAPE))
        desktop_->setCursorShape(QPixmap(), QPoint());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::autosizeWindow()
{
    if (screen_size_.isEmpty())
    {
        LOG(INFO) << "Empty screen size";
        return;
    }

    QScreen* current_screen = window()->screen();
    QRect local_screen_rect = current_screen->availableGeometry();
    QSize window_size = screen_size_ + frameSize() - size();

    if (window_size.width() < local_screen_rect.width() &&
        window_size.height() < local_screen_rect.height())
    {
        QSize remote_screen_size = scaledSize(screen_size_, toolbar_->scale());

        LOG(INFO) << "Show normal (screen_size=" << screen_size_
                  << "local_screen_rect=" << local_screen_rect
                  << "window_size=" << window_size
                  << "remote_screen_size=" << remote_screen_size
                  << ")";
        showNormal();

        resize(remote_screen_size);
        move(local_screen_rect.x() + (local_screen_rect.width() / 2 - window_size.width() / 2),
             local_screen_rect.y() + (local_screen_rect.height() / 2 - window_size.height() / 2));
    }
    else
    {
        LOG(INFO) << "Show maximized (screen_size=" << screen_size_
                  << "local_screen_rect=" << local_screen_rect
                  << "window_size=" << window_size << ")";
        showMaximized();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::takeScreenshot()
{
    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(this,
                                                     tr("Save File"),
                                                     QString(),
                                                     tr("PNG Image (*.png);;BMP Image (*.bmp)"),
                                                     &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "[ACTION] File path not selected";
        return;
    }

    base::FrameQImage* frame = static_cast<base::FrameQImage*>(desktop_->desktopFrame());
    if (!frame)
    {
        LOG(INFO) << "No desktop frame";
        return;
    }

    const char* format = nullptr;

    if (selected_filter.contains(QLatin1String("*.png")))
        format = "PNG";
    else if (selected_filter.contains(QLatin1String("*.bmp")))
        format = "BMP";

    if (!format)
    {
        LOG(INFO) << "File format not selected";
        return;
    }

    if (!frame->constImage().save(file_path, format))
    {
        LOG(ERROR) << "Unable to save image";
        QMessageBox::warning(this, tr("Warning"), tr("Could not save image"), QMessageBox::Ok);
    }
    else
    {
        LOG(INFO) << "Image saved to file:" << file_path;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::scaleDesktop()
{
    if (screen_size_.isEmpty())
    {
        LOG(INFO) << "No screen size";
        return;
    }

    QSize source_size(screen_size_);
    QSize target_size(size());

    int scale = toolbar_->scale();

    if (scale != -1)
    {
        target_size = scaledSize(source_size, scale);
        LOG(INFO) << "Scaling enabled (source_size=" << source_size << "target_size="
                  << target_size << "scale=" << scale << ")";
    }

    desktop_->resize(source_size.scaled(target_size, Qt::KeepAspectRatio));

    if (resize_timer_->isActive())
    {
        LOG(INFO) << "Resize timer stopped";
        resize_timer_->stop();
    }

    LOG(INFO) << "Starting resize timer (scale=" << scale << "size=" << size()
              << "target_size=" << target_size << ")";
    resize_timer_->start(std::chrono::milliseconds(500));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onResizeTimer()
{
    QSize desktop_size = desktop_->size();

    QScreen* current_screen = window()->windowHandle()->screen();
    if (current_screen)
        desktop_size *= current_screen->devicePixelRatio();

    LOG(INFO) << "Resize timer timeout (desktop_size=" << desktop_size << ")";

    emit sig_preferredSizeChanged(desktop_size.width(), desktop_size.height());
    resize_timer_->stop();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onScrollTimer()
{
    if (scroll_delta_.x() != 0)
    {
        QScrollBar* scrollbar = scroll_area_->horizontalScrollBar();

        int pos = scrollbar->sliderPosition() + scroll_delta_.x();

        pos = std::max(pos, scrollbar->minimum());
        pos = std::min(pos, scrollbar->maximum());

        scrollbar->setSliderPosition(pos);
    }

    if (scroll_delta_.y() != 0)
    {
        QScrollBar* scrollbar = scroll_area_->verticalScrollBar();

        int pos = scrollbar->sliderPosition() + scroll_delta_.y();

        pos = std::max(pos, scrollbar->minimum());
        pos = std::min(pos, scrollbar->maximum());

        scrollbar->setSliderPosition(pos);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onPasteKeystrokes()
{
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard)
    {
        QString text = clipboard->text();
        if (text.isEmpty())
        {
            LOG(INFO) << "Empty clipboard";
            return;
        }

        proto::desktop::TextEvent event;
        event.set_text(text.toStdString());

        emit sig_textEvent(event);
    }
    else
    {
        LOG(ERROR) << "QApplication::clipboard failed";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionWindow::onShowHidePanel()
{
    QSize start_panel_size = toolbar_->size();
    QSize end_panel_size = toolbar_->sizeHint();

    int start_x = ((width() * panel_pos_x_) / 100) - (start_panel_size.width() / 2);
    int end_x = ((width() * panel_pos_x_) / 100) - (end_panel_size.width() / 2);

    QPropertyAnimation* animation = new QPropertyAnimation(toolbar_, QByteArrayLiteral("geometry"));
    animation->setStartValue(QRect(QPoint(start_x, 0), start_panel_size));
    animation->setEndValue(QRect(QPoint(end_x, 0), end_panel_size));
    animation->setDuration(150);
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

} // namespace client
