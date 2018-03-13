//
// PROJECT:         Aspia
// FILE:            host/user_list.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/user_list.h"

#include <QSettings>

namespace aspia {

UserList ReadUserList()
{
    UserList user_list;

    QSettings settings(QSettings::SystemScope, "Aspia", "Host");

    int size = settings.beginReadArray("UserList");
    for (int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);

        User user;
        if (!user.setName(settings.value("UserName").toString()))
            return UserList();

        if (!user.setPasswordHash(settings.value("PasswordHash").toByteArray()))
            return UserList();

        user.setFlags(settings.value("Flags").toUInt());
        user.setSessions(settings.value("Sessions").toUInt());

        user_list.push_back(user);
    }
    settings.endArray();

    return user_list;
}

bool WriteUserList(const UserList& user_list)
{
    QSettings settings(QSettings::SystemScope, "Aspia", "Host");

    if (!settings.isWritable())
        return false;

    settings.remove("UserList");

    settings.beginWriteArray("UserList");
    for (int i = 0; i < user_list.size(); ++i)
    {
        settings.setArrayIndex(i);
        settings.setValue("UserName", user_list[i].name());
        settings.setValue("PasswordHash", user_list[i].passwordHash());
        settings.setValue("Flags", user_list[i].flags());
        settings.setValue("Sessions", user_list[i].sessions());
    }
    settings.endArray();

    return true;
}

} // namespace aspia
