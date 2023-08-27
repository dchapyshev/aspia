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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_DIRECT_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_DIRECT_H

#include <deque>
#include <memory>
#include <string>

namespace base {
class Location;
class TaskRunner;
} // namespace base

namespace client {

class OnlineCheckerDirect
{
public:
    explicit OnlineCheckerDirect(std::shared_ptr<base::TaskRunner> task_runner);
    ~OnlineCheckerDirect();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onDirectCheckerResult(int computer_id, bool online) = 0;
        virtual void onDirectCheckerFinished() = 0;
    };

    struct Computer
    {
        int computer_id = -1;
        std::u16string address;
        uint16_t port = 0;
    };
    using ComputerList = std::deque<Computer>;

    void start(const ComputerList& computers, Delegate* delegate);

private:
    void onChecked(int computer_id, bool online);
    void onFinished(const base::Location& location);

    std::shared_ptr<base::TaskRunner> task_runner_;
    ComputerList pending_queue_;
    Delegate* delegate_ = nullptr;

    class Instance;
    std::deque<std::unique_ptr<Instance>> work_queue_;
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_DIRECT_H
