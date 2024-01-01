//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_IPC_IPC_SERVER_H
#define BASE_IPC_IPC_SERVER_H

#include "build/build_config.h"
#include "base/memory/local_memory.h"
#include "base/threading/thread_checker.h"

#include <asio/io_context.hpp>

#include <array>

namespace base {

class IpcChannel;
class Location;

class IpcServer
{
public:
    IpcServer();
    ~IpcServer();

    static std::u16string createUniqueId();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onNewConnection(std::unique_ptr<IpcChannel> channel) = 0;
        virtual void onErrorOccurred() = 0;
    };

    bool start(std::u16string_view channel_id, Delegate* delegate);
    void stop();

private:
    bool runListener(size_t index);
    void onNewConnection(size_t index, std::unique_ptr<IpcChannel> channel);
    void onErrorOccurred(const Location& location);

    Delegate* delegate_ = nullptr;

    asio::io_context& io_context_;
    std::u16string channel_name_;

#if defined(OS_WIN)
    static const size_t kListenersCount = 8;
#elif defined(OS_POSIX)
    static const size_t kListenersCount = 1;
#endif

    class Listener;
    std::array<base::local_shared_ptr<Listener>, kListenersCount> listeners_;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(IpcServer);
};

} // namespace base

#endif // BASE_IPC_IPC_SERVER_H
