//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__QT_DESKTOP_WINDOW_H
#define CLIENT__UI__QT_DESKTOP_WINDOW_H

#include "base/version.h"
#include "client/client_desktop.h"
#include "client/desktop_window.h"
#include "client/ui/client_window.h"
#include "client/ui/desktop_widget.h"
#include "common/clipboard.h"

#include <QPointer>

class QHBoxLayout;
class QScrollArea;

namespace desktop {
class Frame;
} // namespace desktop

namespace client {

class DesktopConfigDialog;
class DesktopPanel;
class SystemInfoWindow;

class QtDesktopWindow :
    public ClientWindow,
    public DesktopWindow,
    public DesktopWidget::Delegate,
    public common::Clipboard::Delegate
{
    Q_OBJECT

public:
    QtDesktopWindow(proto::SessionType session_type,
                    const proto::DesktopConfig& desktop_config,
                    QWidget* parent = nullptr);
    ~QtDesktopWindow();

    // ClientWindow implementation.
    std::unique_ptr<Client> createClient(std::shared_ptr<base::TaskRunner> ui_task_runner) override;

    // DesktopWindow implementation.
    void showWindow(std::shared_ptr<DesktopControlProxy> desktop_control_proxy,
                    const base::Version& peer_version) override;
    void configRequired() override;
    void setCapabilities(const std::string& extensions, uint32_t video_encodings) override;
    void setScreenList(const proto::ScreenList& screen_list) override;
    void setSystemInfo(const proto::SystemInfo& system_info) override;
    std::unique_ptr<FrameFactory> frameFactory() override;
    void drawFrame(std::shared_ptr<desktop::Frame> frame) override;
    void drawMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor) override;
    void injectClipboardEvent(const proto::ClipboardEvent& event) override;

    // DesktopWidget::Delegate implementation.
    void onPointerEvent(const QPoint& pos, uint32_t mask) override;
    void onKeyEvent(uint32_t usb_keycode, uint32_t flags) override;
    void onDrawDesktop() override;

protected:
    // QWidget implementation.
    void timerEvent(QTimerEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

    // common::Clipboard::Delegate implementation.
    void onClipboardEvent(const proto::ClipboardEvent& event) override;

private slots:
    void changeSettings();
    void onConfigChanged(const proto::DesktopConfig& config);
    void autosizeWindow();
    void takeScreenshot();
    void scaleDesktop();

private:
    const proto::SessionType session_type_;
    proto::DesktopConfig desktop_config_;

    std::shared_ptr<DesktopControlProxy> desktop_control_proxy_;
    base::Version peer_version_;
    uint32_t video_encodings_ = 0;

    std::unique_ptr<common::Clipboard> clipboard_;

    QHBoxLayout* layout_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DesktopPanel* panel_ = nullptr;
    DesktopWidget* desktop_ = nullptr;

    QPointer<SystemInfoWindow> system_info_;

    int scroll_timer_id_ = 0;
    QPoint scroll_delta_;

    bool is_maximized_ = false;

    desktop::Point screen_top_left_;

    DISALLOW_COPY_AND_ASSIGN(QtDesktopWindow);
};

} // namespace client

#endif // CLIENT__UI__QT_DESKTOP_WINDOW_H
