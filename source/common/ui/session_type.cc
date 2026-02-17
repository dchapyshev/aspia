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

#include "common/ui/session_type.h"

#include <QCoreApplication>

namespace common {

namespace {

//--------------------------------------------------------------------------------------------------
const char* sessionTypeToString(proto::peer::SessionType session_type)
{
    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
            return QT_TRANSLATE_NOOP("SessionType", "Desktop Manage");

        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
            return QT_TRANSLATE_NOOP("SessionType", "Desktop View");

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            return QT_TRANSLATE_NOOP("SessionType", "File Transfer");

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            return QT_TRANSLATE_NOOP("SessionType", "System Information");

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            return QT_TRANSLATE_NOOP("SessionType", "Text Chat");

        default:
            return "";
    }
}

//--------------------------------------------------------------------------------------------------
const char* sessionTypeToShortString(proto::peer::SessionType session_type)
{
    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
            return QT_TRANSLATE_NOOP("SessionType", "Manage");

        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
            return QT_TRANSLATE_NOOP("SessionType", "View");

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            return QT_TRANSLATE_NOOP("SessionType", "Files");

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            return QT_TRANSLATE_NOOP("SessionType", "Info");

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            return QT_TRANSLATE_NOOP("SessionType", "Chat");

        default:
            return "";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString sessionName(proto::peer::SessionType session_type)
{
    return QCoreApplication::translate("SessionType", sessionTypeToString(session_type));
}

//--------------------------------------------------------------------------------------------------
QString sessionShortName(proto::peer::SessionType session_type)
{
    return QCoreApplication::translate("SessionType", sessionTypeToShortString(session_type));
}

//--------------------------------------------------------------------------------------------------
QIcon sessionIcon(proto::peer::SessionType session_type)
{
    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
            return QIcon(":/img/workstation.svg");

        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
            return QIcon(":/img/computer.svg");

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            return QIcon(":/img/file-explorer.svg");

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            return QIcon(":/img/system-information.svg");

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            return QIcon(":/img/chat.svg");

        default:
            return QIcon();
    }
}

} // namespace common
