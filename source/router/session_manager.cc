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

#include "router/session_manager.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SessionManager::~SessionManager() = default;

//--------------------------------------------------------------------------------------------------
QList<Session*> SessionManager::sessions() const
{
    return sessions_;
}

//--------------------------------------------------------------------------------------------------
void SessionManager::addSession(Session* session)
{
    sessions_.emplace_back(session);
    connect(session, &Session::sig_sessionFinished, this, &SessionManager::onSessionFinished);
}

//--------------------------------------------------------------------------------------------------
bool SessionManager::stopSession(Session::SessionId id)
{
    for (auto it = sessions_.begin(), it_end = sessions_.end(); it != it_end; ++it)
    {
        Session* session = *it;

        if (session->sessionId() == id)
        {
            session->deleteLater();
            sessions_.erase(it);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
Session* SessionManager::sessionById(Session::SessionId session_id)
{
    for (const auto& session : std::as_const(sessions_))
    {
        if (session->sessionId() == session_id)
            return session;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onSessionFinished(Session::SessionId session_id)
{
    for (auto it = sessions_.begin(), it_end = sessions_.end(); it != it_end; ++it)
    {
        Session* session = *it;

        if (session->sessionId() == session_id)
        {
            // Session will be destroyed after completion of the current call.
            session->deleteLater();

            // Delete a session from the list.
            sessions_.erase(it);
            break;
        }
    }
}

} // namespace router
