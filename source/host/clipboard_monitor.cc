//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/clipboard_monitor.h"

#include "base/logging.h"

namespace host {

ClipboardMonitor::ClipboardMonitor()
    : ui_thread_(std::make_unique<base::Thread>())
{
    // Nothing
}

ClipboardMonitor::~ClipboardMonitor()
{
    ui_thread_->stop();
}

void ClipboardMonitor::start(std::shared_ptr<base::TaskRunner> caller_task_runner,
                             common::Clipboard::Delegate* delegate)
{
    caller_task_runner_ = std::move(caller_task_runner);
    delegate_ = delegate;

    DCHECK(caller_task_runner_);
    DCHECK(delegate_);

    ui_thread_->start(base::MessageLoop::Type::WIN, this);
}

void ClipboardMonitor::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!ui_task_runner_)
        return;

    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&ClipboardMonitor::injectClipboardEvent, this, event));
        return;
    }

    if (clipboard_)
        clipboard_->injectClipboardEvent(event);
}

void ClipboardMonitor::onBeforeThreadRunning()
{
    ui_task_runner_ = ui_thread_->taskRunner();
    DCHECK(ui_task_runner_);

    clipboard_ = std::make_unique<common::Clipboard>();
    clipboard_->start(this);
}

void ClipboardMonitor::onAfterThreadRunning()
{
    clipboard_.reset();
}

void ClipboardMonitor::onClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!caller_task_runner_->belongsToCurrentThread())
    {
        caller_task_runner_->postTask(std::bind(&ClipboardMonitor::onClipboardEvent, this, event));
        return;
    }

    if (delegate_)
        delegate_->onClipboardEvent(event);
}

} // namespace host
