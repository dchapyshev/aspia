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

#include <QObject>

#include "router/session.h"

namespace router {

class Session;

class SessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager() final;

    QList<Session*> sessions() const;

    void addSession(Session* session);
    bool stopSession(Session::SessionId id);

    Session* sessionById(Session::SessionId session_id);

private slots:
    void onSessionFinished(Session::SessionId session_id);

private:
    QList<Session*> sessions_;
};

} // namespace router

#endif // ROUTER_SESSION_MANAGER_H
