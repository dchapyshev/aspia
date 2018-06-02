//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_window.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__DESKTOP_WINDOW_H
#define _ASPIA_CLIENT__UI__DESKTOP_WINDOW_H

#include <QPointer>
#include <QWidget>

#include "protocol/address_book.pb.h"

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
    DesktopWindow(proto::address_book::Computer* computer, QWidget* parent = nullptr);
    ~DesktopWindow() = default;

    void resizeDesktopFrame(const QSize& screen_size);
    void drawDesktopFrame();
    DesktopFrame* desktopFrame();
    void injectCursor(const QCursor& cursor);
    void injectClipboard(const proto::desktop::ClipboardEvent& event);

    void setSupportedVideoEncodings(quint32 video_encodings);
    void setSupportedFeatures(quint32 features);
    bool requireConfigChange(proto::desktop::Config* config);

signals:
    void windowClose();
    void sendConfig(const proto::desktop::Config& config);
    void sendKeyEvent(quint32 usb_keycode, quint32 flags);
    void sendPointerEvent(const QPoint& pos, quint32 mask);
    void sendClipboardEvent(const proto::desktop::ClipboardEvent& event);

protected:
    // QWidget implementation.
    void timerEvent(QTimerEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void onPointerEvent(const QPoint& pos, quint32 mask);
    void changeSettings();
    void autosizeWindow();

private:
    proto::address_book::Computer* computer_;

    quint32 supported_video_encodings_ = 0;
    quint32 supported_features_ = 0;

    QPointer<QHBoxLayout> layout_;
    QPointer<QScrollArea> scroll_area_;
    QPointer<DesktopPanel> panel_;
    QPointer<DesktopWidget> desktop_;
    QPointer<Clipboard> clipboard_;

    int scroll_timer_id_ = 0;
    QPoint scroll_delta_;

    bool is_maximized_ = false;

    Q_DISABLE_COPY(DesktopWindow)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__DESKTOP_WINDOW_H
