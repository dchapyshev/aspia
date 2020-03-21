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

#ifndef ROUTER__WIN__ROUTER_SERVICE_H
#define ROUTER__WIN__ROUTER_SERVICE_H

#include "base/win/service.h"

namespace router {

class Server;

class Service : public base::win::Service
{
public:
    Service();
    ~Service();

protected:
    // base::win::Service implementation.
    void onStart() override;
    void onStop() override;
    void onSessionEvent(base::win::SessionStatus event, base::SessionId session_id) override;

private:
    std::unique_ptr<Server> server_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace router

#endif // ROUTER__WIN__ROUTER_SERVICE_H
