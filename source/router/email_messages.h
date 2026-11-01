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

#ifndef ROUTER_EMAIL_MESSAGES_H
#define ROUTER_EMAIL_MESSAGES_H

#include <QString>

#include "base/time_types.h"

class EmailMessages
{
public:
    struct Message
    {
        QString subject;
        QString text;
    };

    static Message twoFactorPassed(const QString& user_name, const QString& address,
        const QString& computer_name, const QString& os_name);

    static Message wrongTwoFactorCode(const QString& user_name, const QString& address,
        const QString& computer_name, const QString& os_name);

    static Message twoFactorBlocked(const QString& user_name, const QString& address,
        const QString& computer_name, const QString& os_name, Minutes block);

private:
    Q_DISABLE_COPY_MOVE(EmailMessages)
};

#endif // ROUTER_EMAIL_MESSAGES_H
