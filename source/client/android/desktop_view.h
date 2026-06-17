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

#ifndef CLIENT_ANDROID_DESKTOP_VIEW_H
#define CLIENT_ANDROID_DESKTOP_VIEW_H

#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QSizeF>
#include <QWidget>

#include <memory>

#include "base/desktop/shared_frame.h"

namespace proto::input {
class MouseEvent;
} // namespace proto::input

class MouseCursor;
class QTimer;
class QTouchEvent;

class DesktopView final : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopView(QWidget* parent = nullptr);
    ~DesktopView() final;

    void setFrame(SharedFrame frame);
    void setCursorShape(std::shared_ptr<MouseCursor> cursor);
    void refresh();

signals:
    void sig_mouseEvent(const proto::input::MouseEvent& event);

protected:
    // QWidget implementation.
    bool event(QEvent* event) final;
    void paintEvent(QPaintEvent* event) final;

private slots:
    void onLongPress();
    void onEdgeScroll();

private:
    void handleTouch(QTouchEvent* event);
    void startDrag();
    void updateEdgeScroll(const QPointF& finger_pos);
    void moveCursorBy(const QPointF& widget_delta);
    void applyZoom(qreal factor, const QPointF& anchor);
    void sendMouse(quint32 mask);
    void sendClick(quint32 mask);
    void sendWheel(int steps);

    // Size of the frame fitted into the widget (aspect ratio preserved) at zoom 1, and the ratio of
    // on-screen pixels to host pixels.
    QSizeF fittedSize() const;
    qreal contentScale() const;

    // Maps a host-frame point to its on-screen position and back.
    QPointF frameToWidget(const QPointF& frame_pos) const;

    void clampContentPos();
    void clampCursor();
    void ensureCursorVisible();

    SharedFrame frame_;
    QImage image_;
    QImage cursor_image_;
    QPoint cursor_hotspot_;

    // Zoom factor (1 = fit to widget), top-left of the scaled frame, and the virtual cursor position
    // in host-frame coordinates.
    qreal zoom_ = 1.0;
    QPointF content_pos_;
    QPointF cursor_pos_;

    // Touch gesture state.
    enum class Gesture { NONE, ONE_FINGER, TWO_FINGER, IGNORED };
    Gesture gesture_ = Gesture::NONE;
    QPointF last_point_;
    bool moved_ = false;
    qreal pinch_distance_ = 0.0;
    QPointF pinch_centroid_;
    bool pinch_moved_ = false;

    // A two-finger gesture is locked to zooming or scrolling once it moves far enough.
    enum class TwoFingerMode { UNDECIDED, ZOOM, SCROLL };
    TwoFingerMode two_finger_mode_ = TwoFingerMode::UNDECIDED;
    qreal two_finger_start_distance_ = 0.0;
    QPointF two_finger_start_centroid_;
    qreal scroll_accumulator_ = 0.0;

    // Buttons currently held while moving the cursor (a tap-then-hold drag presses the left button).
    quint32 button_mask_ = 0;
    bool armed_drag_ = false;
    bool dragging_ = false;
    QElapsedTimer gesture_timer_;
    qint64 last_tap_time_ = -10000;
    QTimer* long_press_timer_ = nullptr;
    QTimer* edge_scroll_timer_ = nullptr;
    QPointF edge_velocity_;

    Q_DISABLE_COPY_MOVE(DesktopView)
};

#endif // CLIENT_ANDROID_DESKTOP_VIEW_H
