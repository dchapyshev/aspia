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

#ifndef HOST_ANDROID_INPUT_INJECTOR_ANDROID_H
#define HOST_ANDROID_INPUT_INJECTOR_ANDROID_H

#include <QPoint>
#include <QPointF>
#include <QSize>

#include "host/input_injector.h"

// Injects input through an Android accessibility service (see InputService.java), the only way to drive
// other applications without root. The service dispatches gestures, so pointer actions are translated
// into taps, long presses and swipes rather than absolute cursor motion (Android has no system mouse
// cursor). Key events are mapped to the few global navigation actions the accessibility API exposes;
// arbitrary key and text injection is not available through this path. The user must enable the service
// in the system accessibility settings; until then injection is a no-op.
class InputInjectorAndroid final : public InputInjector
{
public:
    explicit InputInjectorAndroid(QObject* parent = nullptr);
    ~InputInjectorAndroid() final;

    static InputInjectorAndroid* create(QObject* parent = nullptr);

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

private:
    QSize screen_size_;
    QPoint screen_offset_;

    // Android meta state accumulated from the modifier keys (Shift/Ctrl/Alt/Meta), which arrive as their
    // own key events; applied when translating the following character keys.
    int modifiers_ = 0;

    // Pointer gesture state. A press records the start point; the matching release emits a tap when the
    // pointer barely moved or a swipe otherwise, so clicks and drags both map onto a single gesture.
    bool left_pressed_ = false;
    bool right_pressed_ = false;
    bool left_moved_ = false;
    QPointF left_down_pos_;

    Q_DISABLE_COPY(InputInjectorAndroid)
};

#endif // HOST_ANDROID_INPUT_INJECTOR_ANDROID_H
