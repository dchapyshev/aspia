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

#ifndef ASPIA_CLIENT__UI__DESKTOP_WINDOW_H_
#define ASPIA_CLIENT__UI__DESKTOP_WINDOW_H_

#include <QPointer>
#include <QWidget>

#include "client/connect_data.h"
#include "protocol/desktop_session.pb.h"

class QHBoxLayout;
class QScrollArea;

namespace aspia {

class Clipboard;
class DesktopFrame;
class DesktopPanel;
class DesktopWidget;

class DesktopWindow : public QWidget
{
    Q_OBJECT

public:
    DesktopWindow(ConnectData* connect_data, QWidget* parent = nullptr);
    ~DesktopWindow() = default;

    void resizeDesktopFrame(const QPoint& top_left, const QSize& screen_size);
    void drawDesktopFrame();
    DesktopFrame* desktopFrame();
    void injectCursor(const QCursor& cursor);
    void injectClipboard(const proto::desktop::ClipboardEvent& event);
    void setScreenList(const proto::desktop::ScreenList& screen_list);

    void setSupportedVideoEncodings(uint32_t video_encodings);
    void setSupportedFeatures(uint32_t features);
    bool requireConfigChange(proto::desktop::Config* config);

signals:
    void windowClose();
    void sendConfig(const proto::desktop::Config& config);
    void sendKeyEvent(uint32_t usb_keycode, uint32_t flags);
    void sendPointerEvent(const QPoint& pos, uint32_t mask);
    void sendClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void sendScreen(const proto::desktop::Screen& screen);

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
    void autosizeWindow();

private:
    ConnectData* connect_data_;

    uint32_t supported_video_encodings_ = 0;
    uint32_t supported_features_ = 0;

    QPointer<QHBoxLayout> layout_;
    QPointer<QScrollArea> scroll_area_;
    QPointer<DesktopPanel> panel_;
    QPointer<DesktopWidget> desktop_;
    QPointer<Clipboard> clipboard_;

    int scroll_timer_id_ = 0;
    QPoint scroll_delta_;

    bool is_maximized_ = false;

    QPoint screen_top_left_;

    Q_DISABLE_COPY(DesktopWindow)
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__DESKTOP_WINDOW_H_
