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

#include "client/desktop/desktop/desktop_window.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPalette>
#include <QResizeEvent>
#include <QPropertyAnimation>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QWindow>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/desktop/mouse_cursor.h"
#include "client/settings.h"
#include "client/desktop/desktop/desktop_toolbar.h"
#include "client/desktop/desktop/desktop_widget.h"
#include "client/desktop/desktop/statistics_dialog.h"
#include "client/desktop/desktop/task_manager_window.h"
#include "client/workers/audio_worker.h"
#include "client/workers/network_worker.h"
#include "client/workers/video_worker.h"
#include "common/clipboard.h"
#include "common/desktop_session_constants.h"
#include "common/desktop/msg_box.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_control.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_input.h"
#include "proto/desktop_legacy.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_user.h"
#include "proto/desktop_video.h"
#include "proto/peer.h"
#include "proto/task_manager.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/windows_version.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

const quint32 kWheelMask =
    proto::input::MouseEvent::WHEEL_DOWN | proto::input::MouseEvent::WHEEL_UP;

// Mouse move events are coalesced and flushed at this fixed rate to cap the outgoing event stream
// from high-polling-rate mice. Button and wheel events bypass coalescing and are sent immediately.
const int kMouseFlushIntervalMs = 20; // 50Hz

//--------------------------------------------------------------------------------------------------
QSize scaledSize(const QSize& source_size, int scale)
{
    if (scale == -1)
        return source_size;

    int width = int(double(source_size.width() * scale) / 100.0);
    int height = int(double(source_size.height() * scale) / 100.0);

    return QSize(width, height);
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopWindow::DesktopWindow(const proto::control::Config& desktop_config, QWidget* parent)
    : ClientWindow(proto::peer::SESSION_TYPE_DESKTOP, parent),
      desktop_config_(desktop_config)
{
    LOG(INFO) << "Ctor";

    setMinimumSize(500, 400);

    desktop_ = new DesktopWidget(this);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setAlignment(Qt::AlignCenter);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setWidget(desktop_);
    scroll_area_->viewport()->setStyleSheet("background-color: rgb(25, 25, 25);");

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(scroll_area_);

    toolbar_ = new DesktopToolBar(scroll_area_);
    toolbar_->installEventFilter(this);

    resize_timer_ = new QTimer(this);
    connect(resize_timer_, &QTimer::timeout, this, &DesktopWindow::onResizeTimer);

    scroll_timer_ = new QTimer(this);
    connect(scroll_timer_, &QTimer::timeout, this, &DesktopWindow::onScrollTimer);

    // Keeps running while the mouse moves (no per-tick restart) and stops itself once there is
    // nothing left to flush (see onMouseFlushTimer).
    mouse_timer_ = new QTimer(this);
    mouse_timer_->setInterval(kMouseFlushIntervalMs);
    mouse_timer_->setTimerType(Qt::PreciseTimer);
    connect(mouse_timer_, &QTimer::timeout, this, &DesktopWindow::onMouseFlushTimer);

    Settings settings;
    desktop_->enableKeyCombinations(settings.sendKeyCombinations());
    desktop_->enableRemoteCursorPosition(desktop_config_.cursor_position());

    connect(toolbar_, &DesktopToolBar::sig_keyCombination, desktop_, &DesktopWidget::executeKeyCombination);
    connect(toolbar_, &DesktopToolBar::sig_switchToAutosize, this, &DesktopWindow::onAutosizeWindow);
    connect(toolbar_, &DesktopToolBar::sig_takeScreenshot, this, &DesktopWindow::onTakeScreenshot);
    connect(toolbar_, &DesktopToolBar::sig_scaleChanged, this, &DesktopWindow::onScaleDesktop);
    connect(toolbar_, &DesktopToolBar::sig_minimizeSession, this, [this]()
    {
        LOG(INFO) << "Minimize from full screen";
        is_minimized_from_full_screen_ = true;
        emit sig_minimizeRequested();
    });
    connect(toolbar_, &DesktopToolBar::sig_closeSession, this, &DesktopWindow::close);
    connect(toolbar_, &DesktopToolBar::sig_showHidePanel, this, &DesktopWindow::onShowHidePanel);

    connect(toolbar_, &DesktopToolBar::sig_screenSelected, this, &DesktopWindow::onCurrentScreenChanged);
    connect(toolbar_, &DesktopToolBar::sig_powerControl,
            this, [this](proto::power::Control::Action action, bool wait)
    {
        switch (action)
        {
            case proto::power::Control::ACTION_REBOOT:
            case proto::power::Control::ACTION_REBOOT_SAFE_MODE:
                sessionState()->setAutoReconnect(wait);
                break;

            default:
                break;
        }

        onPowerControl(action);
    });

    connect(toolbar_, &DesktopToolBar::sig_startTaskManager, this, [this]()
    {
        if (!task_manager_)
        {
            task_manager_ = new TaskManagerWindow();
            task_manager_->setAttribute(Qt::WA_DeleteOnClose);

            // The task manager talks to the host directly over its own channel
            // (CHANNEL_ID_TASK_MANAGER == 11) via the network worker.
            connect(networkWorker(), &NetworkWorker::sig_channel_11, task_manager_,
                    &TaskManagerWindow::onNetworkMessage, Qt::QueuedConnection);
            connect(task_manager_, &TaskManagerWindow::sig_sendMessage, networkWorker(),
                    &NetworkWorker::onSendMessage, Qt::QueuedConnection);
        }

        task_manager_->show();
        task_manager_->activateWindow();
    });

    connect(toolbar_, &DesktopToolBar::sig_startStatistics, this, &DesktopWindow::onMetricsRequest);
    connect(toolbar_, &DesktopToolBar::sig_pasteAsKeystrokes, this, &DesktopWindow::onPasteKeystrokes);
    connect(toolbar_, &DesktopToolBar::sig_switchToFullscreen, this, &DesktopWindow::sig_fullscreenRequested);
    connect(toolbar_, &DesktopToolBar::sig_actionsChanged, this, &DesktopWindow::sig_actionsChanged);

    desktop_->installEventFilter(this);
    scroll_area_->viewport()->installEventFilter(this);

    connect(toolbar_, &DesktopToolBar::sig_startSession, this, [this](proto::peer::SessionType session_type)
    {
        emit sig_connectRequested(sessionState()->host(), session_type);
    });

    connect(toolbar_, &DesktopToolBar::sig_recordingStateChanged, this, [this](bool enable)
    {
        QString file_path;

        if (enable)
        {
            Settings settings;
            file_path = settings.recordingPath();
        }

        onRecordingChanged(enable, file_path);
    });

    connect(desktop_, &DesktopWidget::sig_mouseEvent, this, &DesktopWindow::onMouseEvent);
    connect(desktop_, &DesktopWidget::sig_keyEvent, this, &DesktopWindow::onKeyEvent);

    desktop_->setFocus();
}

//--------------------------------------------------------------------------------------------------
DesktopWindow::~DesktopWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onRegisterWorkers()
{
    LOG(INFO) << "Register workers";

    std::unique_ptr<AudioWorker> audio_worker = std::make_unique<AudioWorker>();
    audio_worker_ = audio_worker.get();
    addWorker(std::move(audio_worker));

    std::unique_ptr<VideoWorker> video_worker = std::make_unique<VideoWorker>();
    video_worker_ = video_worker.get();
    addWorker(std::move(video_worker));

    NetworkWorker* network_worker = networkWorker();

    connect(network_worker, &NetworkWorker::sig_channel_2, this, &DesktopWindow::onScreenMessage,
            Qt::QueuedConnection);
    connect(network_worker, &NetworkWorker::sig_channel_1, this, &DesktopWindow::onControlMessage,
            Qt::QueuedConnection);
    connect(network_worker, &NetworkWorker::sig_channel_6, this, &DesktopWindow::onClipboardMessage,
            Qt::QueuedConnection);
    connect(network_worker, &NetworkWorker::sig_channel_10, this, &DesktopWindow::onFileMessage,
            Qt::QueuedConnection);
    connect(network_worker, &NetworkWorker::sig_channel_0, this, &DesktopWindow::onLegacyMessage,
            Qt::QueuedConnection);

    connect(this, &DesktopWindow::sig_cursorConfig, video_worker_, &VideoWorker::onCursorConfig,
            Qt::QueuedConnection);

    connect(video_worker_, &VideoWorker::sig_frameError, this, &DesktopWindow::onFrameError,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_frameChanged, this, &DesktopWindow::onFrameChanged,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_drawFrame, this, &DesktopWindow::onDrawFrame,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_mouseCursorChanged, this, &DesktopWindow::onMouseCursorChanged,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_cursorPositionChanged, this, &DesktopWindow::onCursorPositionChanged,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_h264Disabled, this, &DesktopWindow::onVideoH264Disabled,
            Qt::QueuedConnection);

    connect(toolbar_, &DesktopToolBar::sig_switchSession, this, &DesktopWindow::onSwitchSession,
            Qt::UniqueConnection);

    // Push the initial cursor configuration; refreshed later on every onDesktopConfigChanged.
    emit sig_cursorConfig(desktop_config_.cursor_shape(), desktop_config_.cursor_position());

    // Created only when enabled to avoid monitoring the local clipboard.
    clipboard_.reset(desktop_config_.clipboard() ? Clipboard::create(this) : nullptr);
    if (clipboard_)
    {
        connect(clipboard_, &Clipboard::sig_clipboardEvent, this, &DesktopWindow::onClipboardEvent);
        connect(clipboard_, &Clipboard::sig_localFileListChanged,
                this, &DesktopWindow::onClipboardLocalFileListChanged);
        connect(clipboard_, &Clipboard::sig_fileDataRequest,
                this, &DesktopWindow::onClipboardFileDataRequest);
        clipboard_->start();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onSessionStarted()
{
    LOG(INFO) << "Desktop session started";

    start_time_ = Clock::now();

    // The window outlives a single connection; drop the previous session's transfer before the new
    // one (a reconnect goes through onRegisterWorkers/onSessionStarted again).
    delete clipboard_file_transfer_;
    clipboard_file_transfer_ = new ClipboardFileTransfer(this);

    connect(clipboard_file_transfer_, &ClipboardFileTransfer::sig_sendMessage,
            this, [this](const QByteArray& buffer)
    {
        sendMessage(proto::desktop::CHANNEL_ID_FILE, buffer);
    });

    if (clipboard_)
    {
        connect(clipboard_file_transfer_, &ClipboardFileTransfer::sig_fileDataChunk,
                clipboard_, &Clipboard::addFileData);
    }

    emit sig_showRequested();
    toolbar_->enableTextChat(true);

    if (isLegacy())
        return;

    sendCapabilities();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::setTabbedMode(bool tabbed)
{
    LOG(INFO) << "Set tabbed mode:" << tabbed;
    toolbar_->setTabbedMode(tabbed);
}

//--------------------------------------------------------------------------------------------------
QList<QPair<Tab::ActionRole, QList<QAction*>>> DesktopWindow::tabActionGroups() const
{
    return toolbar_->tabActionGroups();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::applySettings()
{
    LOG(INFO) << "Apply client settings";

    Settings settings;

    desktop_config_ = settings.desktopConfig();
    onDesktopConfigChanged(desktop_config_);

    desktop_->enableRemoteCursorPosition(desktop_config_.cursor_position());
    if (!desktop_config_.cursor_shape())
        desktop_->setCursorShape(QPixmap(), QPoint());

    desktop_->enableKeyCombinations(settings.sendKeyCombinations());
}

//--------------------------------------------------------------------------------------------------
QByteArray DesktopWindow::saveState() const
{
    return toolbar_->saveState();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::restoreState(const QByteArray& state)
{
    toolbar_->restoreState(state);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onCapabilitiesChanged(const proto::control::Capabilities& capabilities)
{
    for (int i = 0; i < capabilities.flag_size(); ++i)
    {
        const proto::control::Capabilities::Flag& flag = capabilities.flag(i);
        const std::string& name = flag.name();
        bool value = flag.value();

        if (name == kFlagOSWindows)
        {
            toolbar_->enableCtrlAltDelFeature(true);
            toolbar_->enableRebootInSafeMode(true);
        }
        else if (name == kFlagPasteAsKeystrokes)
            toolbar_->enablePasteAsKeystrokesFeature(value);
        else if (name == kFlagPowerControl)
            toolbar_->enablePowerControl(value);
        else if (name == kFlagSelectScreen)
            toolbar_->enableScreenSelect(value);
        else if (name == kFlagTaskManager)
            toolbar_->enableTaskManager(value);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onScreenListChanged(const proto::screen::ScreenList& screen_list)
{
    toolbar_->setScreenList(screen_list);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onCursorPositionChanged(const proto::cursor::Position& position)
{
    if (!desktop_->hasFrame())
        return;

    const QSize frame_size = desktop_->frameSize();

    int pos_x = int(double(desktop_->width()) * double(position.x()) / double(frame_size.width()));
    int pos_y = int(double(desktop_->height()) * double(position.y()) / double(frame_size.height()));

    desktop_->setCursorPosition(QPoint(pos_x, pos_y));
    desktop_->update();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onMetricsRequest()
{
    if (!statistics_dialog_)
    {
        LOG(INFO) << "Statistics dialog not created yet";

        statistics_dialog_ = new StatisticsDialog(this);
        statistics_dialog_->setAttribute(Qt::WA_DeleteOnClose);

        QString computer_name = sessionState()->computerName();
        if (computer_name.isEmpty())
            computer_name = sessionState()->hostAddress();

        statistics_dialog_->setWindowTitle(
            statistics_dialog_->windowTitle() + " - " + computer_name);

        connect(statistics_dialog_, &StatisticsDialog::sig_metricsRequired,
                this, &DesktopWindow::onMetricsRequest);

        // Each worker is the source of its own metrics; feed them straight to the dialog slots.
        connect(networkWorker(), &NetworkWorker::sig_metrics,
                statistics_dialog_, &StatisticsDialog::onNetworkMetrics);
        connect(video_worker_, &VideoWorker::sig_metrics,
                statistics_dialog_, &StatisticsDialog::onVideoMetrics);
        connect(audio_worker_, &AudioWorker::sig_metrics,
                statistics_dialog_, &StatisticsDialog::onAudioMetrics);

        statistics_dialog_->show();
        statistics_dialog_->activateWindow();
    }

    // The network, video and audio rows are fed to the dialog directly by the workers; here we push
    // only the session-level counters that no worker owns.
    const std::chrono::seconds duration =
        std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start_time_);

    statistics_dialog_->setDuration(duration);
    statistics_dialog_->setClipboardMetrics(read_clipboard_count_, send_clipboard_count_);
    statistics_dialog_->setMouseMetrics(send_mouse_count_, drop_mouse_count_);
    statistics_dialog_->setKeyMetrics(send_key_count_);
    statistics_dialog_->setTextMetrics(send_text_count_);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onFrameError(proto::video::ErrorCode error_code)
{
    desktop_->setDesktopFrameError(error_code);
    desktop_->update();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onFrameChanged(const QSize& screen_size, SharedFrame frame)
{
    screen_size_ = screen_size;
    LOG(INFO) << "Screen size changed:" << screen_size_;

    bool has_old_frame = desktop_->hasFrame();

    desktop_->setDesktopFrame(std::move(frame));
    onScaleDesktop();

    if (!has_old_frame)
    {
        LOG(INFO) << "Resize window (first frame)";
        onAutosizeWindow();

        // If the parameters indicate that it is necessary to record the connection session, then we
        // start recording.
        Settings settings;
        if (settings.recordSessions())
        {
            LOG(INFO) << "Auto-recording enabled";
            toolbar_->startRecording(true);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onDrawFrame(const QList<QRect>& dirty_rects)
{
    desktop_->drawDesktopFrame(dirty_rects);
    toolbar_->update();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onMouseCursorChanged(std::shared_ptr<MouseCursor> mouse_cursor)
{
    int width = mouse_cursor->width();
    int height = mouse_cursor->height();

    QImage image(reinterpret_cast<const uchar*>(mouse_cursor->constImage().data()), width, height,
                 mouse_cursor->stride(), QImage::Format::Format_ARGB32);

    // The cursor is scaled to the current desktop scale inside the widget. The device pixel ratio
    // of the client screen is applied on top of that by Qt itself.
    desktop_->setCursorShape(QPixmap::fromImage(image),
                             QPoint(mouse_cursor->hotSpotX(), mouse_cursor->hotSpotY()));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onSessionListChanged(const proto::control::SessionList& sessions)
{
    toolbar_->setSessionList(sessions);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onInternalReset()
{
    LOG(INFO) << "Internal reset";

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

    if (resize_timer_)
        resize_timer_->stop();
    screen_size_ = QSize();
    if (scroll_timer_)
        scroll_timer_->stop();
    scroll_delta_ = QPoint();

    if (mouse_timer_)
        mouse_timer_->stop();
    pending_mouse_event_.reset();
    last_pos_x_ = 0;
    last_pos_y_ = 0;
    last_mask_ = 0;
    send_mouse_count_ = 0;
    drop_mouse_count_ = 0;
    send_key_count_ = 0;
    send_text_count_ = 0;

    wheel_angle_ = QPoint();

    desktop_->setDesktopFrame(SharedFrame());
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::resizeEvent(QResizeEvent* event)
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
    onScaleDesktop();

    QWidget::resizeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::leaveEvent(QEvent* event)
{
    if (scroll_timer_->isActive())
        scroll_timer_->stop();

    QWidget::leaveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::showEvent(QShowEvent* event)
{
    if (is_minimized_from_full_screen_ && isWindow())
    {
        LOG(INFO) << "Restore to full screen";
        is_minimized_from_full_screen_ = false;

#if defined(Q_OS_WINDOWS)
        if (windowsVersion() >= VERSION_WIN11)
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
void DesktopWindow::focusOutEvent(QFocusEvent* event)
{
    LOG(INFO) << "Focus out event";
    desktop_->userLeftFromWindow();
    QWidget::focusOutEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event";

    if (task_manager_)
    {
        LOG(INFO) << "Close Task Manager window";
        task_manager_->close();
    }

    ClientWindow::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
bool DesktopWindow::eventFilter(QObject* object, QEvent* event)
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
            // degrees). We accumulate the rotation angle until it becomes 120 degrees. Each axis is
            // accumulated and flushed independently so a diagonal scroll produces separate vertical
            // and horizontal events.
            wheel_angle_ += wheel_event->angleDelta();

            const int step = QWheelEvent::DefaultDeltasPerStep;
            const QPoint pos = desktop_->mapFromGlobal(wheel_event->globalPosition().toPoint());

            if (wheel_angle_.y() >= step || wheel_angle_.y() <= -step)
            {
                desktop_->doMouseEvent(wheel_event->type(), wheel_event->buttons(), pos,
                                       QPoint(0, wheel_angle_.y()));
                wheel_angle_.setY(0);
            }

            if (wheel_angle_.x() >= step || wheel_angle_.x() <= -step)
            {
                desktop_->doMouseEvent(wheel_event->type(), wheel_event->buttons(), pos,
                                       QPoint(wheel_angle_.x(), 0));
                wheel_angle_.setX(0);
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
void DesktopWindow::onMouseEvent(const proto::input::MouseEvent& event)
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

    if (desktop_->hasFrame())
    {
        const QSize source_size = desktop_->frameSize();
        QSize scaled_size = desktop_->size();

        double scale_x = (scaled_size.width() * 100) / static_cast<double>(source_size.width());
        double scale_y = (scaled_size.height() * 100) / static_cast<double>(source_size.height());
        double scale = std::min(scale_x, scale_y);

        proto::input::MouseEvent out_event;

        out_event.set_mask(event.mask());
        out_event.set_x(static_cast<int>(static_cast<double>(pos.x() * 100) / scale));
        out_event.set_y(static_cast<int>(static_cast<double>(pos.y() * 100) / scale));

        // Button and wheel changes must reach the host immediately and in order, so they bypass
        // coalescing. Such an event already carries the latest position, so any queued move is
        // redundant.
        if ((out_event.mask() & ~kWheelMask) != last_mask_ || (out_event.mask() & kWheelMask) != 0)
        {
            pending_mouse_event_.reset();
            sendMouseEvent(out_event);
        }
        else
        {
            // Pure movement: coalesce to the most recent position and flush it on the timer. Every
            // distinct position is kept so the final resting position always reaches the host.
            // A queued move superseded before the flush is counted as dropped.
            if (pending_mouse_event_)
                ++drop_mouse_count_;

            pending_mouse_event_ = out_event;

            if (!mouse_timer_->isActive())
                mouse_timer_->start();
        }
    }

    // In MacOS event Leave does not always come to the widget when the mouse leaves its area.
    if (!toolbar_->isPanelHidden() && !toolbar_->isPanelPinned())
    {
        if (!toolbar_->rect().contains(pos))
            QApplication::postEvent(toolbar_, new QEvent(QEvent::Leave));
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onMouseFlushTimer()
{
    if (!pending_mouse_event_)
    {
        // Nothing moved since the last tick. Stop the repeating timer so it does not wake the system
        // while idle. The next mouse move restarts it.
        mouse_timer_->stop();
        return;
    }

    const proto::input::MouseEvent& event = *pending_mouse_event_;

    // Skip an exact duplicate of the last sent position (scaling can map several local pixels onto
    // the same remote pixel), but never drop a real movement - even a single-pixel one.
    if (event.x() != last_pos_x_ || event.y() != last_pos_y_)
        sendMouseEvent(event);
    else
        ++drop_mouse_count_;

    pending_mouse_event_.reset();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onKeyEvent(const proto::input::KeyEvent& event)
{
    ++send_key_count_;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_key_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
        message.mutable_key()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                    outgoing_message_.serialize<proto::input::ClientToHost>());
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onAutosizeWindow()
{
    if (!isWindow())
        return;

    if (screen_size_.isEmpty())
    {
        LOG(INFO) << "Empty screen size";
        return;
    }

    QWidget* top = window();
    QScreen* current_screen = top->screen();
    QRect local_screen_rect = current_screen->availableGeometry();
    QSize window_size = screen_size_ + top->frameSize() - top->size();

    if (window_size.width() < local_screen_rect.width() &&
        window_size.height() < local_screen_rect.height())
    {
        QSize remote_screen_size = scaledSize(screen_size_, toolbar_->scale());

        LOG(INFO) << "Show normal (screen_size:" << screen_size_
                  << "local_screen_rect:" << local_screen_rect
                  << "window_size:" << window_size
                  << "remote_screen_size:" << remote_screen_size << ")";
        top->showNormal();

        top->resize(remote_screen_size);
        top->move(local_screen_rect.x() + (local_screen_rect.width() / 2 - window_size.width() / 2),
                  local_screen_rect.y() + (local_screen_rect.height() / 2 - window_size.height() / 2));
    }
    else
    {
        LOG(INFO) << "Show maximized (screen_size:" << screen_size_
                  << "local_screen_rect:" << local_screen_rect
                  << "window_size:" << window_size << ")";
        top->showMaximized();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onTakeScreenshot()
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

    const QImage image = desktop_->frameImage();
    if (image.isNull())
    {
        LOG(INFO) << "No desktop frame";
        return;
    }

    const char* format = nullptr;

    if (selected_filter.contains("*.png"))
        format = "PNG";
    else if (selected_filter.contains("*.bmp"))
        format = "BMP";

    if (!format)
    {
        LOG(INFO) << "File format not selected";
        return;
    }

    if (!image.save(file_path, format))
    {
        LOG(ERROR) << "Unable to save image";
        MsgBox::warning(this, tr("Could not save image"));
    }
    else
    {
        LOG(INFO) << "Image saved to file:" << file_path;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onScaleDesktop()
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
void DesktopWindow::onResizeTimer()
{
    QSize desktop_size = desktop_->size();

    QScreen* current_screen = window()->windowHandle()->screen();
    if (current_screen)
        desktop_size *= current_screen->devicePixelRatio();

    LOG(INFO) << "Resize timer timeout (desktop_size=" << desktop_size << ")";

    if (!isLegacy())
    {
        proto::video::ClientToHost message;
        proto::video::PreferredSize* preferred_size = message.mutable_preferred_size();
        preferred_size->set_width(desktop_size.width());
        preferred_size->set_height(desktop_size.height());
        sendMessage(proto::desktop::CHANNEL_ID_VIDEO, serialize(message));
    }

    resize_timer_->stop();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onScrollTimer()
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
void DesktopWindow::onPasteKeystrokes()
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

        ++send_text_count_;

        if (isLegacy())
        {
            proto::legacy::ClientToSession message;
            message.mutable_text_event()->set_text(text.toStdString());
            sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
        }
        else
        {
            proto::input::ClientToHost& message =
                outgoing_message_.newMessage<proto::input::ClientToHost>();
            message.mutable_text()->set_text(text.toStdString());
            sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                        outgoing_message_.serialize<proto::input::ClientToHost>());
        }
    }
    else
    {
        LOG(ERROR) << "QApplication::clipboard failed";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onShowHidePanel()
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

//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendMouseEvent(const proto::input::MouseEvent& event)
{
    last_pos_x_ = event.x();
    last_pos_y_ = event.y();
    last_mask_ = event.mask() & ~kWheelMask;

    ++send_mouse_count_;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_mouse_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
        message.mutable_mouse()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                    outgoing_message_.serialize<proto::input::ClientToHost>());
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onScreenMessage(const QByteArray& buffer)
{
    proto::screen::HostToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse screen message";
        return;
    }

    if (message.has_screen_list())
        onScreenListChanged(message.screen_list());
    else
        LOG(WARNING) << "Unhandled screen message";
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onControlMessage(const QByteArray& buffer)
{
    proto::control::HostToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse control message";
        return;
    }

    if (message.has_capabilities())
    {
        readCapabilities(message.capabilities());
    }
    else if (message.has_session_list())
    {
        LOG(INFO) << "Received:" << message.session_list();
        onSessionListChanged(message.session_list());
    }
    else
    {
        LOG(ERROR) << "Unhandled service message from host";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onClipboardMessage(const QByteArray& buffer)
{
    proto::clipboard::HostToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse clipboard message";
        return;
    }

    if (message.has_event())
        readClipboardEvent(message.event());
    else
        LOG(ERROR) << "Unhandled clipboard message from host";
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onFileMessage(const QByteArray& buffer)
{
    if (clipboard_file_transfer_)
        clipboard_file_transfer_->onIncomingMessage(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onLegacyMessage(const QByteArray& buffer)
{
    // The media workers pick out video, audio and cursor updates from this channel themselves; the
    // GUI thread handles only clipboard, capabilities and extensions.
    proto::legacy::SessionToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Invalid session message from host";
        return;
    }

    if (message.has_clipboard_event())
        readClipboardEvent(message.clipboard_event());
    else if (message.has_capabilities())
        readLegacyCapabilities(message.capabilities());
    else if (message.has_extension())
        readExtension(message.extension());
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onDesktopConfigChanged(const proto::control::Config& config)
{
    desktop_config_ = config;
    emit sig_cursorConfig(desktop_config_.cursor_shape(), desktop_config_.cursor_position());
    sendConfig(desktop_config_);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onCurrentScreenChanged(const proto::screen::Screen& screen)
{
    LOG(INFO) << "Current screen changed:" << screen.id();

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        extension->set_name(kSelectScreenExtension);
        extension->set_data(screen.SerializeAsString());
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::screen::ClientToHost message;
        message.mutable_screen()->CopyFrom(screen);
        sendMessage(proto::desktop::CHANNEL_ID_SCREEN, serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onRecordingChanged(bool enable, const QString& file_path)
{
    proto::user::ClientToHost message;
    proto::user::VideoRecording* video_recording = message.mutable_video_recording();

    if (enable)
    {
        LOG(INFO) << "Video recording is enabled:" << file_path;
        video_recording->set_action(proto::user::VideoRecording::ACTION_STARTED);
    }
    else
    {
        LOG(INFO) << "Video recording is disabled";
        video_recording->set_action(proto::user::VideoRecording::ACTION_STOPPED);
    }

    // The video worker owns the output file and re-encodes decoded frames straight from their YUV
    // form while recording is on.
    const QString computer_name = sessionState()->computerName();
    QMetaObject::invokeMethod(video_worker_,
        [worker = video_worker_, enable, file_path, computer_name]()
    {
        worker->onSetRecording(enable, file_path, computer_name);
    }, Qt::QueuedConnection);

    if (isLegacy())
        return;

    sendMessage(proto::desktop::CHANNEL_ID_USER, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onPowerControl(proto::power::Control::Action action)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        proto::power::Control power_control;
        power_control.set_action(action);
        extension->set_name(kPowerControlExtension);
        extension->set_data(power_control.SerializeAsString());
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::power::ClientToHost message;
        message.mutable_power_control()->set_action(action);
        sendMessage(proto::desktop::CHANNEL_ID_POWER, serialize(message));
    }
}


//--------------------------------------------------------------------------------------------------
void DesktopWindow::onSwitchSession(quint32 session_id)
{
    proto::control::ClientToHost message;
    proto::control::SwitchSession* switch_session = message.mutable_switch_session();
    switch_session->set_session_id(session_id);

    LOG(INFO) << "Send:" << *switch_session;
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onClipboardEvent(const proto::clipboard::Event& event)
{
    if (!desktop_config_.clipboard())
        return;

    ++send_clipboard_count_;

    if (event.mime_type() == Clipboard::kMimeTypeFileList.toStdString() &&
        !file_clipboard_supported_)
    {
        LOG(WARNING) << "File clipboard is not supported by remote side, skipping file list";
        return;
    }

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_clipboard_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::clipboard::ClientToHost message;
        message.mutable_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_CLIPBOARD, serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onClipboardLocalFileListChanged(const QVector<LocalFileEntry>& files)
{
    if (clipboard_file_transfer_)
        clipboard_file_transfer_->setLocalFileList(files);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onClipboardFileDataRequest(int file_index)
{
    if (clipboard_file_transfer_)
        clipboard_file_transfer_->requestFileData(file_index);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::onVideoH264Disabled()
{
    h264_sw_enabled_ = false;
    sendCapabilities();
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::readCapabilities(const proto::control::Capabilities& capabilities)
{
    LOG(INFO) << "Received:" << capabilities;

    for (int i = 0; i < capabilities.flag_size(); ++i)
    {
        const proto::control::Capabilities::Flag& flag = capabilities.flag(i);
        if (flag.name() == kFlagFileClipboard)
        {
            file_clipboard_supported_ = flag.value();
            break;
        }
    }

    onCapabilitiesChanged(capabilities);
    sendConfig(desktop_config_);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::readLegacyCapabilities(const proto::legacy::Capabilities& legacy_capabilities)
{
    LOG(INFO) << "Received:" << legacy_capabilities;

    proto::control::Capabilities capabilities;

    auto add_flag = [&capabilities](const char* name, bool value)
    {
        proto::control::Capabilities::Flag* flag = capabilities.add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

    if (legacy_capabilities.os_type() == proto::legacy::Capabilities::OS_TYPE_WINDOWS)
        add_flag(kFlagOSWindows, true);

    quint32 video_encodings = legacy_capabilities.video_encodings();
    if (video_encodings & proto::video::ENCODING_VP8)
        add_flag(kFlagVideoVP8, true);
    if (video_encodings & proto::video::ENCODING_VP9)
        add_flag(kFlagVideoVP9, true);

    quint32 audio_encodings = legacy_capabilities.audio_encodings();
    if (audio_encodings & proto::audio::ENCODING_OPUS)
        add_flag(kFlagAudioOpus, true);

    // Convert legacy "disable_*" flags (inverted logic) to new positive flags.
    // If a disable flag is absent in the legacy message, the feature is considered enabled.
    bool paste_as_keystrokes = true;
    bool clipboard = true;
    bool cursor_shape = true;
    bool cursor_position = true;
    bool desktop_effects = true;
    bool desktop_wallpaper = true;
    bool lock_at_disconnect = true;
    bool block_input = true;

    for (int i = 0; i < legacy_capabilities.flag_size(); ++i)
    {
        const proto::legacy::Capabilities::Flag& flag = legacy_capabilities.flag(i);
        const std::string& name = flag.name();
        bool value = flag.value();

        if (name == kFlagDisablePasteAsKeystrokes)
            paste_as_keystrokes = !value;
        else if (name == kFlagDisableClipboard)
            clipboard = !value;
        else if (name == kFlagDisableCursorShape)
            cursor_shape = !value;
        else if (name == kFlagDisableCursorPosition)
            cursor_position = !value;
        else if (name == kFlagDisableDesktopEffects)
            desktop_effects = !value;
        else if (name == kFlagDisableDesktopWallpaper)
            desktop_wallpaper = !value;
        else if (name == kFlagDisableLockAtDisconnect)
            lock_at_disconnect = !value;
        else if (name == kFlagDisableBlockInput)
            block_input = !value;
    }

    add_flag(kFlagPasteAsKeystrokes, paste_as_keystrokes);
    add_flag(kFlagClipboard, clipboard);
    add_flag(kFlagCursorShape, cursor_shape);
    add_flag(kFlagCursorPosition, cursor_position);
    add_flag(kFlagDesktopEffects, desktop_effects);
    add_flag(kFlagDesktopWallpaper, desktop_wallpaper);
    add_flag(kFlagLockAtDisconnect, lock_at_disconnect);
    add_flag(kFlagBlockInput, block_input);

    // Convert legacy extensions string (semicolon-separated) to new flags.
    QString extensions = QString::fromStdString(legacy_capabilities.extensions());
    QStringList extension_list = extensions.split(';', Qt::SkipEmptyParts);

    add_flag(kFlagSelectScreen, extension_list.contains(kSelectScreenExtension));
    add_flag(kFlagPowerControl, extension_list.contains(kPowerControlExtension));

    LOG(INFO) << "Converted:" << capabilities;
    onCapabilitiesChanged(capabilities);
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::readExtension(const proto::legacy::Extension& extension)
{
    if (!isLegacy())
        return;

    if (extension.name() == kSelectScreenExtension)
    {
        proto::screen::ScreenList screen_list;
        if (!screen_list.ParseFromString(extension.data()))
        {
            LOG(ERROR) << "Unable to parse select screen extension data";
            return;
        }

        LOG(INFO) << "Screen list receive:" << screen_list;
        onScreenListChanged(screen_list);
    }
    else
    {
        LOG(ERROR) << "Unknown extension:" << extension.name();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::readClipboardEvent(const proto::clipboard::Event& event)
{
    if (event.mime_type() == Clipboard::kMimeTypeFileList.toStdString() &&
        !file_clipboard_supported_)
    {
        LOG(WARNING) << "File clipboard is not supported by remote side, ignoring file list";
        return;
    }

    if (desktop_config_.clipboard())
    {
        ++read_clipboard_count_;
        if (clipboard_)
            clipboard_->injectClipboardEvent(event);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendSessionListRequest()
{
    proto::control::ClientToHost message;
    proto::control::SessionsRequest* request = message.mutable_sessions_request();
    request->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendConfig(const proto::control::Config& config)
{
    LOG(INFO) << "Send:" << config;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Config* legacy_config = message.mutable_config();

        quint32 flags = proto::legacy::NO_FLAGS;
        if (config.cursor_shape())
            flags |= proto::legacy::ENABLE_CURSOR_SHAPE;
        if (config.clipboard())
            flags |= proto::legacy::ENABLE_CLIPBOARD;
        if (!config.effects())
            flags |= proto::legacy::DISABLE_EFFECTS;
        if (!config.wallpaper())
            flags |= proto::legacy::DISABLE_WALLPAPER;
        if (config.block_input())
            flags |= proto::legacy::BLOCK_REMOTE_INPUT;
        if (config.lock_at_disconnect())
            flags |= proto::legacy::LOCK_AT_DISCONNECT;
        if (config.cursor_position())
            flags |= proto::legacy::CURSOR_POSITION;

        legacy_config->set_flags(flags);
        legacy_config->set_video_encoding(proto::video::ENCODING_VP8);
        legacy_config->set_audio_encoding(
            config.audio() ? proto::audio::ENCODING_OPUS : proto::audio::ENCODING_UNKNOWN);

        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::control::ClientToHost message;
        message.mutable_config()->CopyFrom(desktop_config_);
        sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
        sendSessionListRequest();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWindow::sendCapabilities()
{
    proto::control::ClientToHost message;
    proto::control::Capabilities* capabilities = message.mutable_capabilities();

    auto add_flag = [capabilities](const char* name, bool value)
    {
        proto::control::Capabilities::Flag* flag = capabilities->add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

    add_flag(kFlagVideoVP8, true);
    add_flag(kFlagVideoVP9, true);
    if (h264_sw_enabled_)
        add_flag(kFlagVideoH264, true);
    add_flag(kFlagAudioOpus, true);

#if defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)
    add_flag(kFlagFileClipboard, true);
#endif

    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

