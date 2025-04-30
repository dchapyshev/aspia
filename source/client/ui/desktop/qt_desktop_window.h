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

#ifndef CLIENT_UI_DESKTOP_QT_DESKTOP_WINDOW_H
#define CLIENT_UI_DESKTOP_QT_DESKTOP_WINDOW_H

#include "base/desktop/mouse_cursor.h"
#include "client/client_desktop.h"
#include "client/ui/session_window.h"
#include "client/ui/desktop/desktop_widget.h"

#include <QPointer>

class QHBoxLayout;
class QScrollArea;

namespace desktop {
class Frame;
} // namespace desktop

namespace client {

class DesktopConfigDialog;
class DesktopToolBar;
class QtSystemInfoWindow;
class StatisticsDialog;
class TaskManagerWindow;

class QtDesktopWindow final : public SessionWindow
{
    Q_OBJECT

public:
    QtDesktopWindow(proto::SessionType session_type,
                    const proto::DesktopConfig& desktop_config,
                    QWidget* parent = nullptr);
    ~QtDesktopWindow() final;

    // SessionWindow implementation.
    Client* createClient() final;

public slots:
    void showWindow();
    void configRequired();
    void setCapabilities(const proto::DesktopCapabilities& capabilities);
    void setScreenList(const proto::ScreenList& screen_list);
    void setScreenType(const proto::ScreenType& screen_type);
    void setCursorPosition(const proto::CursorPosition& cursor_position);
    void setSystemInfo(const proto::system_info::SystemInfo& system_info);
    void setTaskManager(const proto::task_manager::HostToClient& message);
    void setMetrics(const client::ClientDesktop::Metrics& metrics);
    void setFrameError(proto::VideoErrorCode error_code);
    void setFrame(const base::Size& screen_size, std::shared_ptr<base::Frame> frame);
    void drawFrame();
    void setMouseCursor(std::shared_ptr<base::MouseCursor> mouse_cursor);

signals:
    void sig_desktopConfigChanged(const proto::DesktopConfig& config);
    void sig_screenSelected(const proto::Screen& screen);
    void sig_preferredSizeChanged(int width, int height);
    void sig_videoPaused(bool enable);
    void sig_audioPaused(bool enable);
    void sig_videoRecording(bool enable, const std::filesystem::path& file_path);
    void sig_keyEvent(const proto::KeyEvent& event);
    void sig_textEvent(const proto::TextEvent& event);
    void sig_mouseEvent(const proto::MouseEvent& event);
    void sig_powerControl(proto::PowerControl::Action action);
    void sig_remoteUpdate();
    void sig_systemInfoRequested(const proto::system_info::SystemInfoRequest& request);
    void sig_taskManager(const proto::task_manager::ClientToHost& message);
    void sig_metricsRequested();

protected:
    // SessionWindow implementation.
    void onInternalReset() final;

    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;
    void leaveEvent(QEvent* event) final;
    void changeEvent(QEvent* event) final;
    void showEvent(QShowEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;
    void closeEvent(QCloseEvent* event) final;
    bool eventFilter(QObject* object, QEvent* event) final;

private slots:
    void onMouseEvent(const proto::MouseEvent& event);
    void changeSettings();
    void onConfigChanged(const proto::DesktopConfig& config);
    void autosizeWindow();
    void takeScreenshot();
    void scaleDesktop();
    void onResizeTimer();
    void onScrollTimer();
    void onPasteKeystrokes();
    void onShowHidePanel();

private:
    const proto::SessionType session_type_;
    proto::DesktopConfig desktop_config_;

    uint32_t video_encodings_ = 0;

    QHBoxLayout* layout_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DesktopToolBar* toolbar_ = nullptr;
    DesktopWidget* desktop_ = nullptr;

    QPointer<QtSystemInfoWindow> system_info_;
    QPointer<TaskManagerWindow> task_manager_;
    QPointer<StatisticsDialog> statistics_dialog_;

    QTimer* resize_timer_ = nullptr;
    QSize screen_size_;
    QTimer* scroll_timer_ = nullptr;
    QPoint scroll_delta_;

    bool is_maximized_ = false;
    bool is_minimized_from_full_screen_ = false;

    std::optional<QPoint> start_panel_pos_;
    int panel_pos_x_ = 50;

    bool enable_video_pause_ = true;
    bool video_pause_last_ = false;
    bool enable_audio_pause_ = true;
    bool audio_pause_last_ = false;

    bool disable_feature_audio_ = false;
    bool disable_feature_clipboard_ = false;
    bool disable_feature_cursor_shape_ = false;
    bool disable_feature_cursor_position_ = false;
    bool disable_feature_desktop_effects_ = false;
    bool disable_feature_desktop_wallpaper_ = false;
    bool disable_feature_font_smoothing_ = false;
    bool disable_feature_clear_clipboard_ = false;
    bool disable_feature_lock_at_disconnect_ = false;
    bool disable_feature_block_input_ = false;

    QPoint wheel_angle_;

    DISALLOW_COPY_AND_ASSIGN(QtDesktopWindow);
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_QT_DESKTOP_WINDOW_H
