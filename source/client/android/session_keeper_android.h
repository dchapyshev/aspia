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

#ifndef CLIENT_ANDROID_SESSION_KEEPER_ANDROID_H
#define CLIENT_ANDROID_SESSION_KEEPER_ANDROID_H

#include "client/session_keeper.h"

class SessionKeeperAndroid final : public SessionKeeper
{
    Q_OBJECT

public:
    explicit SessionKeeperAndroid(QObject* parent = nullptr);
    ~SessionKeeperAndroid() final = default;

    // SessionKeeper implementation.
    void acquire() final;
    void release() final;

private:
    Q_DISABLE_COPY_MOVE(SessionKeeperAndroid)
};

#endif // CLIENT_ANDROID_SESSION_KEEPER_ANDROID_H
