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

#include "host/input_thread.h"

#include "host/input_injector.h"

namespace host {

InputThread::InputThread(QObject* parent, bool block_input)
    : QThread(parent),
      block_input_(block_input)
{
    start(QThread::HighPriority);
}

InputThread::~InputThread()
{
    {
        std::scoped_lock lock(input_queue_lock_);
        terminate_ = true;
        input_event_.notify_one();
    }

    wait();
}

void InputThread::injectPointerEvent(const proto::desktop::PointerEvent& event)
{
    std::scoped_lock lock(input_queue_lock_);
    incoming_input_queue_.emplace(event);
    input_event_.notify_one();
}

void InputThread::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    std::scoped_lock lock(input_queue_lock_);
    incoming_input_queue_.emplace(event);
    input_event_.notify_one();
}

void InputThread::run()
{
    std::unique_ptr<InputInjector> injector = std::make_unique<InputInjector>(block_input_);

    while (true)
    {
        std::queue<InputEvent> work_input_queue;

        {
            std::unique_lock lock(input_queue_lock_);

            while (incoming_input_queue_.empty() && !terminate_)
                input_event_.wait(lock);

            if (terminate_)
                return;

            work_input_queue.swap(incoming_input_queue_);
        }

        while (!work_input_queue.empty())
        {
            const InputEvent& input_event = work_input_queue.front();

            if (std::holds_alternative<proto::desktop::KeyEvent>(input_event))
            {
                injector->injectKeyEvent(std::get<proto::desktop::KeyEvent>(input_event));
            }
            else if (std::holds_alternative<proto::desktop::PointerEvent>(input_event))
            {
                injector->injectPointerEvent(std::get<proto::desktop::PointerEvent>(input_event));
            }

            work_input_queue.pop();
        }
    }
}

} // namespace host
