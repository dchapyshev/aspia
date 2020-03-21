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

#ifndef IPC__IPC_SERVER_H
#define IPC__IPC_SERVER_H

#include "base/macros_magic.h"
#include "base/threading/thread_checker.h"

#include <asio/io_context.hpp>

#include <array>

namespace base {
class Location;
} // namespace base

namespace ipc {

class Channel;

class Server
{
public:
    Server();
    ~Server();

    static std::u16string createUniqueId();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onNewConnection(std::unique_ptr<Channel> channel) = 0;
        virtual void onErrorOccurred() = 0;
    };

    bool start(std::u16string_view channel_id, Delegate* delegate);
    void stop();

private:
    bool runListener(size_t index);
    void onNewConnection(size_t index, std::unique_ptr<Channel> channel);
    void onErrorOccurred(const base::Location& location);

    Delegate* delegate_ = nullptr;

    asio::io_context& io_context_;
    std::u16string channel_name_;

    class Listener;
    std::array<std::shared_ptr<Listener>, 8> listeners_;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace ipc

#endif // IPC__IPC_SERVER_H
