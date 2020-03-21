//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__UI__ROUTER_H
#define ROUTER__UI__ROUTER_H

#include "base/macros_magic.h"

#include <QObject>

namespace router {

class Router : public QObject
{
    Q_OBJECT

public:
    explicit Router(QObject* parent = nullptr);
    ~Router();

    void disconnectOne(const QString& peer_id);
    void disconnectAll();
    void refresh();

    void addProxy();
    void modifyProxy();
    void deleteProxy();

    void addUser();
    void modifyUser();
    void deleteUser();

private:
    DISALLOW_COPY_AND_ASSIGN(Router);
};

} // namespace router

#endif // ROUTER__UI__ROUTER_H
