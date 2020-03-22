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

#ifndef PROXY__SESSION_H
#define PROXY__SESSION_H

#include "base/macros_magic.h"

#include <asio/ip/tcp.hpp>

namespace proxy {

class Session
{
public:
    explicit Session(std::pair<asio::ip::tcp::socket, asio::ip::tcp::socket>&& sockets);
    ~Session();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onSessionFinished(Session* session) = 0;
    };

    void start(Delegate* delegate);
    void stop();

    std::chrono::seconds duration() const;
    int32_t bytesTransferred() const;

private:
    static void doReadSome(Session* session, int source);
    void onErrorOccurred();

    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    int64_t bytes_transferred_ = 0;

    static const int kNumberOfSides = 2;
    static const int kBufferSize = 8192;

    asio::ip::tcp::socket socket_[kNumberOfSides];
    std::array<uint8_t, kBufferSize> buffer_[kNumberOfSides];

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Session);
};

} // namespace proxy

#endif // PROXY__SESSION_H
