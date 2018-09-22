//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_HOST__HOST_SERVICE_H_
#define ASPIA_HOST__HOST_SERVICE_H_

#include <QCoreApplication>

#include "base/service.h"

namespace aspia {

class HostServer;
class ScopedCOMInitializer;

class HostService : public Service<QCoreApplication>
{
public:
    HostService();
    ~HostService();

protected:
    // Service implementation.
    void start() override;
    void stop() override;
    void sessionChange(uint32_t event, uint32_t session_id) override;

private:
    std::unique_ptr<ScopedCOMInitializer> com_initializer_;
    std::unique_ptr<HostServer> server_;

    DISALLOW_COPY_AND_ASSIGN(HostService);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SERVICE_H_
