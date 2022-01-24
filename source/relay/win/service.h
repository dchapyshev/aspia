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

#ifndef RELAY__WIN__SERVICE_H
#define RELAY__WIN__SERVICE_H

#include "base/win/service.h"

namespace relay {

class Controller;

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
    void onPowerEvent(uint32_t event) override;

private:
    std::unique_ptr<Controller> controller_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace relay

#endif // RELAY__WIN__SERVICE_H
