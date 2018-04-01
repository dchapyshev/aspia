//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_window.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__DESKTOP_WINDOW_H
#define _ASPIA_CLIENT__UI__DESKTOP_WINDOW_H

#include <QPointer>
#include <QWidget>

#include "protocol/computer.pb.h"

class QHBoxLayout;
class QScrollArea;

namespace aspia {

class DesktopFrame;
class DesktopPanel;
class DesktopWidget;

class DesktopWindow : public QWidget
{
    Q_OBJECT

public:
    DesktopWindow(proto::Computer* computer, QWidget* parent = nullptr);
    ~DesktopWindow() = default;

    void resizeDesktopFrame(const QSize& screen_size);
    void drawDesktopFrame();
    DesktopFrame* desktopFrame();
    void injectCursor(const QCursor& cursor);
    void injectClipboard(const QString& text);

signals:
    void windowClose();
    void sendConfig(const proto::desktop::Config& config);
    void sendKeyEvent(quint32 usb_keycode, quint32 flags);
    void sendPointerEvent(const QPoint& pos, quint32 mask);
    void sendClipboardEvent(const QString& text);

protected:
    // QWidget implementation.
    void timerEvent(QTimerEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPointerEvent(const QPoint& pos, quint32 mask);
    void clipboardDataChanged();
    void changeSettings();
    void autosizeWindow();

private:
    proto::Computer* computer_;

    QPointer<QHBoxLayout> layout_;
    QPointer<QScrollArea> scroll_area_;
    QPointer<DesktopPanel> panel_;
    QPointer<DesktopWidget> desktop_;

    int scroll_timer_id_ = 0;
    QPoint scroll_delta_;

    bool is_maximized_ = false;

    Q_DISABLE_COPY(DesktopWindow)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__DESKTOP_WINDOW_H
