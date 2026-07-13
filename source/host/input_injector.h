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

#ifndef HOST_INPUT_INJECTOR_H
#define HOST_INPUT_INJECTOR_H

#include <QObject>
#include <QPoint>
#include <QSize>

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
class TouchEvent;
} // namespace proto::input

class InputInjector : public QObject
{
    Q_OBJECT

public:
    enum class Type
    {
        UNKNOWN    = 0,
        WINDOWS    = 1,
        MAC        = 2,
        X11        = 3,
        UINPUT     = 4,
        VT         = 5,
        WAYLAND    = 6,
        ANDROID_OS = 7
    };
    Q_ENUM(Type)

    InputInjector(Type type, QObject* parent)
        : QObject(parent),
          type_(type)
    {
        // Nothing
    }
    virtual ~InputInjector() = default;

    Type type() const { return type_; }

    virtual void setScreenInfo(const QSize& screen_size, const QPoint& offset) = 0;
    virtual void setBlockInput(bool enable) = 0;
    virtual void injectKeyEvent(const proto::input::KeyEvent& event) = 0;
    virtual void injectTextEvent(const proto::input::TextEvent& event) = 0;
    virtual void injectMouseEvent(const proto::input::MouseEvent& event) = 0;
    virtual void injectTouchEvent(const proto::input::TouchEvent& event) = 0;

private:
    const Type type_;
};

#endif // HOST_INPUT_INJECTOR_H
