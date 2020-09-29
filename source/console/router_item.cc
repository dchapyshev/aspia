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

#include "console/router_item.h"

namespace console {

RouterItem::RouterItem(const QString& name, const QString& guid)
    : guid_(guid)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/server.png"));
}

RouterItem::~RouterItem() = default;

QString RouterItem::guid() const
{
    return guid_;
}

} // namespace console
