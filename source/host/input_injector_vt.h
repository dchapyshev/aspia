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

#ifndef HOST_INPUT_INJECTOR_VT_H
#define HOST_INPUT_INJECTOR_VT_H

#include <memory>

#include "host/input_injector.h"

class VtSession;

// Feeds keystrokes into a VtSession's virtual terminal. Key events (USB HID codes) are translated to the
// byte sequences a terminal expects and injected into the VT; mouse and touch input are ignored. This is
// the input side of the VT console fallback, paired with ScreenCapturerVt.
class InputInjectorVt final : public InputInjector
{
public:
    explicit InputInjectorVt(std::shared_ptr<VtSession> session, QObject* parent = nullptr);
    ~InputInjectorVt() final = default;

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

private:
    std::shared_ptr<VtSession> session_;
    bool shift_ = false;
    bool ctrl_ = false;

    Q_DISABLE_COPY_MOVE(InputInjectorVt)
};

#endif // HOST_INPUT_INJECTOR_VT_H
