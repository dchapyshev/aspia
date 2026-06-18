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
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSizeF>
#include <QVariant>
#include <QWidget>

#include <memory>

#include "base/desktop/shared_frame.h"

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
} // namespace proto::input

class MouseCursor;
class QInputMethodEvent;
class QKeyEvent;
class QShowEvent;
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

    // Moves the virtual cursor to the host's reported cursor position (host-frame coordinates).
    void setCursorPosition(const QPointF& position);

    void refresh(const QList<QRect>& dirty_rects);

    void showSoftwareKeyboard();

    // Height (widget pixels) of the key bar shown above the keyboard, so it is excluded from the
    // area used to keep the remote cursor visible.
    void setKeyBarHeight(int height);

    // Sends the Ctrl+Alt+Del sequence (recognized by a Windows host).
    void sendCtrlAltDelete();

public slots:
    void onModifierToggled(quint32 usb_keycode, bool active);
    void onSpecialKey(quint32 usb_keycode);

signals:
    void sig_keyEvent(const proto::input::KeyEvent& event);
    void sig_textEvent(const proto::input::TextEvent& event);
    void sig_mouseEvent(const proto::input::MouseEvent& event);
    void sig_keyboardInsetChanged(int inset);
    void sig_modifiersCleared();

protected:
    // QWidget implementation.
    bool event(QEvent* event) final;
    void paintEvent(QPaintEvent* event) final;
    void keyPressEvent(QKeyEvent* event) final;
    void keyReleaseEvent(QKeyEvent* event) final;
    void showEvent(QShowEvent* event) final;
    void inputMethodEvent(QInputMethodEvent* event) final;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const final;

private slots:
    void onLongPress();
    void onEdgeScroll();
    void onKeyboardRectangleChanged();

private:
    void handleTouch(QTouchEvent* event);
    void startDrag();
    void updateEdgeScroll(const QPointF& finger_pos);
    void moveCursorBy(const QPointF& widget_delta);
    void applyZoom(qreal factor, const QPointF& anchor);
    void sendKey(QKeyEvent* event, bool pressed);
    void sendRawKey(quint32 usb_keycode, bool pressed);
    void sendKeyWithModifiers(quint32 usb_keycode);
    void sendMouse(quint32 mask);
    void sendClick(quint32 mask);
    void sendWheel(int steps);

    // Size of the frame fitted into the widget (aspect ratio preserved) at zoom 1, and the ratio of
    // on-screen pixels to host pixels.
    QSizeF fittedSize() const;
    qreal contentScale() const;

    // Height of the area not covered by the on-screen keyboard, used as the visible bottom for
    // panning so the remote cursor stays above the keyboard.
    qreal viewportHeight() const;

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

    // Height (widget pixels) of the on-screen keyboard overlapping the bottom of the view, and of
    // the key bar shown above it.
    int keyboard_inset_ = 0;
    int key_bar_height_ = 0;

    // Modifiers from the key bar held down for the next key, applied once and then released.
    QList<quint32> sticky_modifiers_;

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
