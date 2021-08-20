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
#include "client/ui/session_window.h"
#include "client/ui/desktop_widget.h"

#include <QPointer>

class QHBoxLayout;
class QScrollArea;

namespace desktop {
class Frame;
} // namespace desktop

namespace client {

class DesktopConfigDialog;
class DesktopPanel;
class DesktopWindowProxy;
class SystemInfoWindow;
class StatisticsDialog;

class QtDesktopWindow :
    public SessionWindow,
    public DesktopWindow
{
    Q_OBJECT

public:
    QtDesktopWindow(proto::SessionType session_type,
                    const proto::DesktopConfig& desktop_config,
                    QWidget* parent = nullptr);
    ~QtDesktopWindow();

    // SessionWindow implementation.
    std::unique_ptr<Client> createClient() override;

    // DesktopWindow implementation.
    void showWindow(std::shared_ptr<DesktopControlProxy> desktop_control_proxy,
                    const base::Version& peer_version) override;
    void configRequired() override;
    void setCapabilities(const std::string& extensions, uint32_t video_encodings) override;
    void setScreenList(const proto::ScreenList& screen_list) override;
    void setSystemInfo(const proto::SystemInfo& system_info) override;
    void setMetrics(const DesktopWindow::Metrics& metrics) override;
    std::unique_ptr<FrameFactory> frameFactory() override;
    void setFrame(const base::Size& screen_size, std::shared_ptr<base::Frame> frame) override;
    void drawFrame() override;
    void setMouseCursor(std::shared_ptr<base::MouseCursor> mouse_cursor) override;

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void onMouseEvent(const proto::MouseEvent& event);
    void onKeyEvent(const proto::KeyEvent& event);
    void changeSettings();
    void onConfigChanged(const proto::DesktopConfig& config);
    void autosizeWindow();
    void takeScreenshot();
    void scaleDesktop();
    void onResizeTimer();
    void onScrollTimer();

private:
    const proto::SessionType session_type_;
    proto::DesktopConfig desktop_config_;

    std::shared_ptr<DesktopWindowProxy> desktop_window_proxy_;
    std::shared_ptr<DesktopControlProxy> desktop_control_proxy_;
    base::Version peer_version_;
    uint32_t video_encodings_ = 0;

    QHBoxLayout* layout_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DesktopPanel* panel_ = nullptr;
    DesktopWidget* desktop_ = nullptr;

    QPointer<SystemInfoWindow> system_info_;
    QPointer<StatisticsDialog> statistics_dialog_;

    QTimer* resize_timer_ = nullptr;
    QSize screen_size_;
    QTimer* scroll_timer_ = nullptr;
    QPoint scroll_delta_;

    bool is_maximized_ = false;

    DISALLOW_COPY_AND_ASSIGN(QtDesktopWindow);
};

} // namespace client

#endif // CLIENT__UI__QT_DESKTOP_WINDOW_H
