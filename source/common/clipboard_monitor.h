//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_CLIPBOARD_MONITOR_H
#define COMMON_CLIPBOARD_MONITOR_H

#include "base/threading/asio_thread.h"
#include "common/clipboard.h"

namespace common {

class ClipboardMonitor final
    : public base::AsioThread::Delegate,
      public common::Clipboard::Delegate
{
public:
    ClipboardMonitor();
    ~ClipboardMonitor() final;

    void start(std::shared_ptr<base::TaskRunner> caller_task_runner,
               common::Clipboard::Delegate* delegate);

    void injectClipboardEvent(const proto::ClipboardEvent& event);
    void clearClipboard();

protected:
    // base::AsioThread::Delegate implementation.
    void onBeforeThreadRunning() final;
    void onAfterThreadRunning() final;

    // common::Clipboard::Delegate implementation.
    void onClipboardEvent(const proto::ClipboardEvent& event) final;

private:
    common::Clipboard::Delegate* delegate_ = nullptr;

    std::unique_ptr<base::AsioThread> thread_;
    std::shared_ptr<base::TaskRunner> caller_task_runner_;
    std::shared_ptr<base::TaskRunner> self_task_runner_;
    std::unique_ptr<common::Clipboard> clipboard_;

    DISALLOW_COPY_AND_ASSIGN(ClipboardMonitor);
};

} // namespace common

#endif // COMMON_CLIPBOARD_MONITOR_H
