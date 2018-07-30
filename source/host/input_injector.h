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

#ifndef ASPIA_HOST__INPUT_INJECTOR_H_
#define ASPIA_HOST__INPUT_INJECTOR_H_

#include <QThread>

#include <condition_variable>
#include <queue>
#include <mutex>
#include <variant>

#include "base/macros_magic.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class InputInjector : public QThread
{
    Q_OBJECT

public:
    InputInjector(QObject* parent);
    ~InputInjector();

    void injectPointerEvent(const proto::desktop::PointerEvent& event);
    void injectKeyEvent(const proto::desktop::KeyEvent& event);

protected:
    // QThread implementation.
    void run() override;

private:
    using InputEvent = std::variant<proto::desktop::PointerEvent, proto::desktop::KeyEvent>;

    std::condition_variable input_event_;
    std::mutex input_queue_lock_;
    std::queue<InputEvent> incoming_input_queue_;
    bool terminate_ = false;

    DISALLOW_COPY_AND_ASSIGN(InputInjector);
};

} // namespace aspia

#endif // ASPIA_HOST__INPUT_INJECTOR_H_
