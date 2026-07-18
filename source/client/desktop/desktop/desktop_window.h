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

#ifndef CLIENT_DESKTOP_DESKTOP_DESKTOP_WINDOW_H
#define CLIENT_DESKTOP_DESKTOP_DESKTOP_WINDOW_H

#include <QPointer>

#include <chrono>
#include <optional>

#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "base/desktop/shared_frame.h"
#include "client/desktop/client_window.h"
#include "client/workers/audio_worker.h"
#include "client/workers/video_worker.h"
#include "common/clipboard.h"
#include "common/clipboard_file_transfer.h"
#include "proto/desktop_control.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_input.h"
#include "proto/desktop_power.h"
#include "proto/desktop_screen.h"

namespace proto::audio {
class Packet;
} // namespace proto::audio

namespace proto::clipboard {
class Event;
} // namespace proto::clipboard

namespace proto::legacy {
class Capabilities;
class Extension;
} // namespace proto::legacy

namespace proto::video {
class Packet;
} // namespace proto::video

class MouseCursor;
class QHBoxLayout;
class QScrollArea;
class QTimer;
class RecordWorker;

class DesktopToolBar;
class DesktopWidget;
class StatisticsDialog;
class TaskManagerWindow;

class DesktopWindow final : public ClientWindow
{
    Q_OBJECT

public:
    DesktopWindow(const proto::control::Config& desktop_config, QWidget* parent = nullptr);
    ~DesktopWindow() final;

    // ClientWindow implementation.
    void setTabbedMode(bool tabbed) final;
    QList<QPair<Tab::ActionRole, QList<QAction*>>> tabActionGroups() const final;
    void applySettings() final;
    QByteArray saveState() const final;
    void restoreState(const QByteArray& state) final;

signals:
    // Pushes the cursor configuration to the video worker.
    void sig_cursorConfig(bool shape_enabled, bool position_enabled);

protected:
    // ClientWindow implementation.
    void onInternalReset() final;
    void onRegisterWorkers() final;
    void onSessionStarted() final;

    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;
    void leaveEvent(QEvent* event) final;
    void showEvent(QShowEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;
    void closeEvent(QCloseEvent* event) final;
    bool eventFilter(QObject* object, QEvent* event) final;

private slots:
    // Incoming session channels (video/audio/cursor are parsed by the workers).
    void onScreenMessage(const QByteArray& buffer);
    void onControlMessage(const QByteArray& buffer);
    void onClipboardMessage(const QByteArray& buffer);
    void onFileMessage(const QByteArray& buffer);
    void onLegacyMessage(const QByteArray& buffer);

    // Video worker feedback.
    void onFrameError(proto::video::ErrorCode error_code);
    void onFrameChanged(const QSize& screen_size, SharedFrame frame);
    void onDrawFrame(const QList<QRect>& dirty_rects);
    void onMouseCursorChanged(std::shared_ptr<MouseCursor> mouse_cursor);
    void onCursorPositionChanged(const proto::cursor::Position& position);
    void onVideoH264Disabled();

    // Outgoing user actions.
    void onCurrentScreenChanged(const proto::screen::Screen& screen);
    void onRecordingChanged(bool enable, const QString& file_path);
    void onPowerControl(proto::power::Control::Action action);
    void onSwitchSession(quint32 session_id);
    void onClipboardEvent(const proto::clipboard::Event& event);
    void onClipboardLocalFileListChanged(const QVector<LocalFileEntry>& files);
    void onClipboardFileDataRequest(int file_index);
    void onMetricsRequest();

    void onMouseEvent(const proto::input::MouseEvent& event);
    void onMouseFlushTimer();
    void onKeyEvent(const proto::input::KeyEvent& event);
    void onAutosizeWindow();
    void onTakeScreenshot();
    void onScaleDesktop();
    void onResizeTimer();
    void onScrollTimer();
    void onPasteKeystrokes();
    void onShowHidePanel();

private:
    void sendMouseEvent(const proto::input::MouseEvent& event);
    void sendConfig(const proto::control::Config& config);
    void sendCapabilities();
    void sendSessionListRequest();
    void onDesktopConfigChanged(const proto::control::Config& config);
    void readCapabilities(const proto::control::Capabilities& capabilities);
    void readLegacyCapabilities(const proto::legacy::Capabilities& capabilities);
    void readExtension(const proto::legacy::Extension& extension);
    void readClipboardEvent(const proto::clipboard::Event& event);
    void onCapabilitiesChanged(const proto::control::Capabilities& capabilities);
    void onScreenListChanged(const proto::screen::ScreenList& screen_list);
    void onSessionListChanged(const proto::control::SessionList& sessions);

    proto::control::Config desktop_config_;

    QHBoxLayout* layout_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DesktopToolBar* toolbar_ = nullptr;
    DesktopWidget* desktop_ = nullptr;

    QPointer<TaskManagerWindow> task_manager_;
    QPointer<StatisticsDialog> statistics_dialog_;
    ScopedQPointer<Clipboard> clipboard_;

    QPointer<AudioWorker> audio_worker_;
    QPointer<VideoWorker> video_worker_;
    QPointer<RecordWorker> record_worker_;
    ClipboardFileTransfer* clipboard_file_transfer_ = nullptr;

    Serializer<proto::input::ClientToHost> outgoing_message_;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    TimePoint start_time_;

    QTimer* resize_timer_ = nullptr;
    QSize screen_size_;
    QTimer* scroll_timer_ = nullptr;
    QPoint scroll_delta_;

    QTimer* mouse_timer_ = nullptr;
    std::optional<proto::input::MouseEvent> pending_mouse_event_;
    qint32 last_pos_x_ = 0;
    qint32 last_pos_y_ = 0;
    quint32 last_mask_ = 0;
    int send_mouse_count_ = 0;
    int drop_mouse_count_ = 0;
    int send_key_count_ = 0;
    int send_text_count_ = 0;
    int read_clipboard_count_ = 0;
    int send_clipboard_count_ = 0;

    bool file_clipboard_supported_ = false;

    // Cleared when the video worker reports a permanent H264 failure. sendCapabilities() drops
    // kFlagVideoH264 so the host switches to VP.
    bool h264_sw_enabled_ = true;

    bool is_minimized_from_full_screen_ = false;

    std::optional<QPoint> start_panel_pos_;
    int panel_pos_x_ = 50;

    QPoint wheel_angle_;

    Q_DISABLE_COPY_MOVE(DesktopWindow)
};

#endif // CLIENT_DESKTOP_DESKTOP_DESKTOP_WINDOW_H
