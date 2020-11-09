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

#ifndef RELAY__SESSION_H
#define RELAY__SESSION_H

#include "base/macros_magic.h"

#include <asio/ip/tcp.hpp>

namespace base {
class Location;
} // namespace base

namespace relay {

class Session
{
public:
    explicit Session(std::pair<asio::ip::tcp::socket, asio::ip::tcp::socket>&& sockets);
    ~Session();

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onSessionFinished(Session* session) = 0;
    };

    void start(Delegate* delegate);
    void stop();

    std::chrono::seconds idleTime(const TimePoint& current_time) const;
    std::chrono::seconds duration() const;
    int64_t bytesTransferred() const;

private:
    static void doReadSome(Session* session, int source);
    void onErrorOccurred(const base::Location& location, const std::error_code& error_code);

    TimePoint start_time_;
    mutable TimePoint start_idle_time_;
    int64_t bytes_transferred_ = 0;

    static const int kNumberOfSides = 2;
    static const int kBufferSize = 8192;

    asio::ip::tcp::socket socket_[kNumberOfSides];
    std::array<uint8_t, kBufferSize> buffer_[kNumberOfSides];

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Session);
};

} // namespace relay

#endif // RELAY__SESSION_H
