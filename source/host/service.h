//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_SERVICE_H
#define HOST_SERVICE_H

#include "base/service.h"

namespace host {

class Server;

class Service : public base::Service
{
public:
    Service();
    ~Service() override;

protected:
    // base::Service implementation.
    void onStart() override;
    void onStop() override;
    void onSessionEvent(base::win::SessionStatus status, base::SessionId session_id) override;
    void onPowerEvent(uint32_t event) override;

private:
    std::unique_ptr<Server> server_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace host

#endif // HOST_SERVICE_H
