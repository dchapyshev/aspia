//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_NET_SHARE_ENUMERATOR_H
#define BASE_WIN_NET_SHARE_ENUMERATOR_H

#include <QObject>
#include <QString>

#include <qt_windows.h>
#include <LM.h>

namespace base {

class NetShareEnumerator
{
    Q_GADGET

public:
    NetShareEnumerator();
    ~NetShareEnumerator();

    bool isAtEnd() const;
    void advance();

    enum class Type
    {
        UNKNOWN   = 0,
        DISK      = 1,
        PRINTER   = 2,
        DEVICE    = 3,
        IPC       = 4,
        SPECIAL   = 5,
        TEMPORARY = 6
    };
    Q_ENUM(Type)

    QString name() const;
    QString localPath() const;
    QString description() const;
    Type type() const;
    quint32 currentUses() const;
    quint32 maxUses() const;

private:
    PSHARE_INFO_502 share_info_ = nullptr;
    DWORD total_entries_ = 0;
    DWORD current_pos_ = 0;
};

} // namespace base

#endif // BASE_WIN_NET_SHARE_ENUMERATOR_H
