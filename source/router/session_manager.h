//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER_SESSION_MANAGER_H
#define ROUTER_SESSION_MANAGER_H

#include "router/session_client.h"

namespace router {

class SessionManager : public SessionClient
{
    Q_OBJECT

public:
    explicit SessionManager(base::TcpChannel* channel, QObject* parent = nullptr);
    ~SessionManager() override;

protected:
    // Session implementation.
    void onSessionMessage(quint8 channel_id, const QByteArray& buffer) override;

private:
    Q_DISABLE_COPY_MOVE(SessionManager)
};

} // namespace router

#endif // ROUTER_SESSION_MANAGER_H
