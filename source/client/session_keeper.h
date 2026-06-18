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

#ifndef CLIENT_SESSION_KEEPER_H
#define CLIENT_SESSION_KEEPER_H

#include <QObject>

// Best-effort request to keep the process running in the background while a session is active. The
// actual guarantee (its duration and mechanism) is platform-defined; create() returns nullptr on
// platforms that need no keeper.
class SessionKeeper : public QObject
{
    Q_OBJECT

public:
    explicit SessionKeeper(QObject* parent);
    ~SessionKeeper() override = default;

    // Creates the keeper implementation for the current platform (nullptr if none is needed).
    static SessionKeeper* create(QObject* parent = nullptr);

    // Asks the OS to keep the process active in the background.
    virtual void acquire() = 0;

    // Drops the request, letting the OS reclaim the process as usual.
    virtual void release() = 0;

private:
    Q_DISABLE_COPY_MOVE(SessionKeeper)
};

#endif // CLIENT_SESSION_KEEPER_H
