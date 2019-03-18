//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__INPUT_THREAD_H
#define HOST__INPUT_THREAD_H

#include "base/macros_magic.h"
#include "proto/desktop.pb.h"

#include <QThread>

#include <condition_variable>
#include <queue>
#include <mutex>
#include <variant>

namespace host {

class InputThread : public QThread
{
    Q_OBJECT

public:
    explicit InputThread(bool block_input);
    ~InputThread();

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
    const bool block_input_;

    DISALLOW_COPY_AND_ASSIGN(InputThread);
};

} // namespace host

#endif // HOST__INPUT_THREAD_H
