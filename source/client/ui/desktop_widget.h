//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_widget.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__DESKTOP_WIDGET_H
#define _ASPIA_CLIENT__UI__DESKTOP_WIDGET_H

#include <QEvent>
#include <QWidget>
#include <memory>

#include "desktop_capture/desktop_frame.h"

namespace aspia {

class DesktopFrameQImage;

class DesktopWidget : public QWidget
{
    Q_OBJECT

public:
    DesktopWidget(QWidget* parent);
    ~DesktopWidget() = default;

    void resizeDesktopFrame(const QSize& screen_size);
    void drawDesktopFrame();
    DesktopFrame* desktopFrame();

public slots:
    void executeKeySequense(int key_sequence);

signals:
    void sendKeyEvent(quint32 usb_keycode, quint32 flags);
    void sendPointerEvent(const QPoint& pos, quint32 mask);

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void processMouseEvent(QEvent::Type event_type,
                           const Qt::MouseButtons& buttons,
                           const QPoint& pos,
                           const QPoint& delta = QPoint());
    void processKeyEvent(QKeyEvent* event);

    std::unique_ptr<DesktopFrameQImage> frame_;

    QPoint prev_pos_;
    quint32 prev_mask_ = 0;

    Q_DISABLE_COPY(DesktopWidget)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__DESKTOP_WIDGET_H
