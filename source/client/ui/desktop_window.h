//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__UI__DESKTOP_WINDOW_H
#define ASPIA_CLIENT__UI__DESKTOP_WINDOW_H

#include "client/ui/session_window.h"
#include "client/client_session_desktop.h"
#include "protocol/desktop_session.pb.h"
#include "protocol/desktop_session_extensions.pb.h"

class QHBoxLayout;
class QScrollArea;

namespace aspia {

class Clipboard;
class DesktopFrame;
class DesktopPanel;
class DesktopWidget;

class DesktopWindow :
    public SessionWindow,
    public ClientSessionDesktop::Delegate
{
    Q_OBJECT

public:
    DesktopWindow(const ConnectData& connect_data, QWidget* parent);
    ~DesktopWindow() = default;

    // ClientSessionDesktop::Delegate implementation.
    void resizeDesktopFrame(const QRect& screen_rect) override;
    void drawDesktopFrame() override;
    DesktopFrame* desktopFrame() override;
    void injectCursor(const QCursor& cursor) override;
    void injectClipboard(const proto::desktop::ClipboardEvent& event) override;
    void setScreenList(const proto::desktop::ScreenList& screen_list) override;

signals:
    void windowClose();

protected:
    // QWidget implementation.
    void timerEvent(QTimerEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void onPointerEvent(const QPoint& pos, uint32_t mask);
    void changeSettings();
    void onConfigChanged(const proto::desktop::Config& config);
    void autosizeWindow();
    void takeScreenshot();
    void onScalingChanged(bool enabled = true);

private:
    QHBoxLayout* layout_;
    QScrollArea* scroll_area_;
    DesktopPanel* panel_;
    DesktopWidget* desktop_;
    Clipboard* clipboard_;

    int scroll_timer_id_ = 0;
    QPoint scroll_delta_;

    bool is_maximized_ = false;

    QPoint screen_top_left_;

    DISALLOW_COPY_AND_ASSIGN(DesktopWindow);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__DESKTOP_WINDOW_H
