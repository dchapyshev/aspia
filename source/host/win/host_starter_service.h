//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__WIN__HOST_STARTER_SERVICE_H
#define HOST__WIN__HOST_STARTER_SERVICE_H

#include "base/service.h"

#include <QCoreApplication>

namespace host {

class StarterService : public base::Service<QCoreApplication>
{
public:
    StarterService(const QString& service_id);
    ~StarterService();

    static bool startFromService(const QString& program,
                                 const QStringList& arguments,
                                 const QString& session_id);

protected:
    // base::Service implementation.
    void start() override;
    void stop() override;
    void sessionEvent(base::win::SessionStatus status, base::win::SessionId session_id) override;

private:
    DISALLOW_COPY_AND_ASSIGN(StarterService);
};

} // namespace host

#endif // HOST__WIN__HOST_STARTER_SERVICE_H
