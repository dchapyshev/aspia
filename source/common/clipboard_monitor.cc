//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/clipboard_monitor.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "common/clipboard_win.h"
#elif defined(OS_LINUX)
#include "common/clipboard_x11.h"
#elif defined(OS_MAC)
#include "common/clipboard_mac.h"
#else
#error Not implemented
#endif

namespace common {

//--------------------------------------------------------------------------------------------------
ClipboardMonitor::ClipboardMonitor()
    : thread_(std::make_unique<base::Thread>())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ClipboardMonitor::~ClipboardMonitor()
{
    thread_->stop();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::start(std::shared_ptr<base::TaskRunner> caller_task_runner,
                             common::Clipboard::Delegate* delegate)
{
    caller_task_runner_ = std::move(caller_task_runner);
    delegate_ = delegate;

    DCHECK(caller_task_runner_);
    DCHECK(delegate_);

    base::MessageLoop::Type message_loop_type;

#if defined(OS_WIN)
    message_loop_type = base::MessageLoop::Type::WIN;
#elif defined(OS_LINUX)
    message_loop_type = base::MessageLoop::Type::ASIO;
#elif defined(OS_MAC)
    message_loop_type = base::MessageLoop::Type::DEFAULT;
#else
#error Not implemented
#endif

    thread_->start(message_loop_type, this);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!self_task_runner_)
        return;

    if (!self_task_runner_->belongsToCurrentThread())
    {
        self_task_runner_->postTask(
            std::bind(&ClipboardMonitor::injectClipboardEvent, this, event));
        return;
    }

    if (clipboard_)
        clipboard_->injectClipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::clearClipboard()
{
    if (!self_task_runner_)
        return;

    if (!self_task_runner_->belongsToCurrentThread())
    {
        self_task_runner_->postTask(std::bind(&ClipboardMonitor::clearClipboard, this));
        return;
    }

    if (clipboard_)
        clipboard_->clearClipboard();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onBeforeThreadRunning()
{
    self_task_runner_ = thread_->taskRunner();
    DCHECK(self_task_runner_);

#if defined(OS_WIN)
    clipboard_ = std::make_unique<common::ClipboardWin>();
#elif defined(OS_LINUX)
    clipboard_ = std::make_unique<common::ClipboardX11>();
#elif defined(OS_MAC)
    clipboard_ = std::make_unique<common::ClipboardMac>();
#else
#error Not implemented
#endif
    clipboard_->start(this);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMonitor::onAfterThreadRunning()
{
    clipboard_.reset();
}

//--------------------------------------------------------------------------------------------------
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

} // namespace common
