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

#include "router/email_messages.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString clientDetails(const QString& address, const QString& computer_name, const QString& os_name)
{
    return QString("Address: %1\nComputer: %2\nOperating system: %3\n\n").arg(address, computer_name, os_name);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
EmailMessages::Message EmailMessages::twoFactorPassed(const QString& user_name, const QString& address,
    const QString& computer_name, const QString& os_name)
{
    Message message;
    message.subject = "New login to the account";
    message.text = QString("A device logged in to the account \"%1\".\n\n").arg(user_name);
    message.text += clientDetails(address, computer_name, os_name);
    message.text += "If it was not you, change the password of the account.";
    return message;
}

//--------------------------------------------------------------------------------------------------
// static
EmailMessages::Message EmailMessages::wrongTwoFactorCode(const QString& user_name, const QString& address,
    const QString& computer_name, const QString& os_name)
{
    Message message;
    message.subject = "Failed login to the account";
    message.text = QString("Someone entered the correct password of the account \"%1\" but did not confirm "
                           "the login.\n\n").arg(user_name);
    message.text += clientDetails(address, computer_name, os_name);
    message.text += "If it was not you, change the password of the account.";
    return message;
}

//--------------------------------------------------------------------------------------------------
// static
EmailMessages::Message EmailMessages::twoFactorBlocked(const QString& user_name, const QString& address,
    const QString& computer_name, const QString& os_name, Minutes block)
{
    Message message;
    message.subject = "Logins to the account are blocked";
    message.text = QString("Someone entered the correct password of the account \"%1\" but failed to confirm "
                           "the login too many times. Logins to the account are blocked for %2 minutes.\n\n")
                       .arg(user_name, QString::number(block.count()));
    message.text += clientDetails(address, computer_name, os_name);
    message.text += "If it was not you, change the password of the account.";
    return message;
}
