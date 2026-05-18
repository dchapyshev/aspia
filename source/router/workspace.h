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

#ifndef ROUTER_WORKSPACE_H
#define ROUTER_WORKSPACE_H

#include <QByteArray>
#include <QString>

class Workspace
{
public:
    Workspace() = default;
    ~Workspace() = default;

    Workspace(const Workspace& other) = default;
    Workspace& operator=(const Workspace& other) = default;

    Workspace(Workspace&& other) noexcept = default;
    Workspace& operator=(Workspace&& other) noexcept = default;

    static const qsizetype kMaxNameLength = 64;

    static bool isValidName(const QString& name);

    bool isValid() const;

    struct Access
    {
        qint64 workspace_id = 0;
        qint64 user_id = 0;
        QByteArray wrapped_gk;
    };

    qint64 entry_id = 0;
    QString name;
};

#endif // ROUTER_WORKSPACE_H
