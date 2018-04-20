//
// PROJECT:         Aspia
// FILE:            host/user_list.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__USER_LIST_H
#define _ASPIA_HOST__USER_LIST_H

#include <QVector>

#include "host/user.h"

namespace aspia {

using UserList = QVector<User>;

UserList readUserList();
bool writeUserList(const UserList& user_list);

} // namespace aspia

#endif // _ASPIA_HOST__USER_LIST_H
