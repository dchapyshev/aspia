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

#include <optional>

#include "client/client_desktop.h"
#include "base/desktop/shared_frame.h"
#include "client/desktop/client_window.h"

class MouseCursor;
class QHBoxLayout;
class QScrollArea;

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
    Client* createClient() final;
    void setTabbedMode(bool tabbed) final;
    QList<QPair<Tab::ActionRole, QList<QAction*>>> tabActionGroups() const final;
    void applySettings() final;
    QByteArray saveState() const final;
    void restoreState(const QByteArray& state) final;

public slots:
    void onShowWindow();
    void onCapabilitiesChanged(const proto::control::Capabilities& capabilities);
    void onScreenListChanged(const proto::screen::ScreenList& screen_list);
    void onCursorPositionChanged(const proto::cursor::Position& position);
    void onTaskManagerChanged(const proto::task_manager::HostToClient& message);
    void onMetricsChanged(const ClientDesktop::Metrics& metrics);
    void onFrameError(proto::video::ErrorCode error_code);
    void onFrameChanged(const QSize& screen_size, SharedFrame frame);
    void onDrawFrame(const QList<QRect>& dirty_rects);
    void onMouseCursorChanged(std::shared_ptr<MouseCursor> mouse_cursor);
    void onSessionListChanged(const proto::control::SessionList& sessions);

signals:
    void sig_desktopConfigChanged(const proto::control::Config& config);
    void sig_screenSelected(const proto::screen::Screen& screen);
    void sig_preferredSizeChanged(int width, int height);
    void sig_videoRecording(bool enable, const QString& file_path);
    void sig_keyEvent(const proto::input::KeyEvent& event);
    void sig_textEvent(const proto::input::TextEvent& event);
    void sig_mouseEvent(const proto::input::MouseEvent& event);
    void sig_powerControl(proto::power::Control_Action action);
    void sig_taskManager(const proto::task_manager::ClientToHost& message);
    void sig_metricsRequested();

protected:
    // ClientWindow implementation.
    void onInternalReset() final;

    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;
    void leaveEvent(QEvent* event) final;
    void showEvent(QShowEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;
    void closeEvent(QCloseEvent* event) final;
    bool eventFilter(QObject* object, QEvent* event) final;

private slots:
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

    proto::control::Config desktop_config_;

    QHBoxLayout* layout_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DesktopToolBar* toolbar_ = nullptr;
    DesktopWidget* desktop_ = nullptr;

    QPointer<TaskManagerWindow> task_manager_;
    QPointer<StatisticsDialog> statistics_dialog_;

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

    bool is_minimized_from_full_screen_ = false;

    std::optional<QPoint> start_panel_pos_;
    int panel_pos_x_ = 50;

    QPoint wheel_angle_;

    Q_DISABLE_COPY_MOVE(DesktopWindow)
};

#endif // CLIENT_DESKTOP_DESKTOP_DESKTOP_WINDOW_H
