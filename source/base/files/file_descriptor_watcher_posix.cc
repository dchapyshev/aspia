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

#include "base/files/file_descriptor_watcher_posix.h"

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"

#include <asio/posix/descriptor.hpp>

namespace base {

class FileDescriptorWatcher::Watcher : public asio::posix::descriptor
{
public:
    Watcher(int fd, Mode mode, const Callback& callback);
    ~Watcher();

    void start();

private:
    Callback callback_;
    asio::posix::descriptor::wait_type wait_type_;

    Q_DISABLE_COPY(Watcher)
};

//--------------------------------------------------------------------------------------------------
FileDescriptorWatcher::Watcher::Watcher(int fd, Mode mode, const Callback& callback)
    : asio::posix::descriptor(base::AsioEventDispatcher::currentIoContext()),
      callback_(callback)
{
    DCHECK(callback_);

    switch (mode)
    {
        case Mode::WATCH_READ:
            wait_type_ = asio::posix::descriptor::wait_read;
            break;

        case Mode::WATCH_WRITE:
            wait_type_ = asio::posix::descriptor::wait_write;
            break;

        default:
            NOTREACHED();
            break;
    }

    std::error_code ignored_error;
    assign(fd, ignored_error);
}

//--------------------------------------------------------------------------------------------------
FileDescriptorWatcher::Watcher::~Watcher()
{
    // We do not own the descriptor and should not close it. We cancel the asynchronous operation
    // and release the descriptor.
    cancel();
    release();
}

//--------------------------------------------------------------------------------------------------
void FileDescriptorWatcher::Watcher::start()
{
    async_wait(wait_type_, [this](const std::error_code& error_code)
    {
        if (error_code == asio::error::operation_aborted)
            return;

        if (error_code)
        {
            LOG(ERROR) << "FD watcher error:" << error_code;
        }
        else
        {
            callback_();
        }

        // Restart watcher.
        start();
    });
}

//--------------------------------------------------------------------------------------------------
FileDescriptorWatcher::FileDescriptorWatcher() = default;

//--------------------------------------------------------------------------------------------------
FileDescriptorWatcher::~FileDescriptorWatcher() = default;

//--------------------------------------------------------------------------------------------------
void FileDescriptorWatcher::startWatching(int fd, Mode mode, const Callback& callback)
{
    impl_ = std::make_unique<Watcher>(fd, mode, callback);
    impl_->start();
}

} // namespace base
