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

#include "client/ui/router_tree_item.h"

namespace client {

RouterTreeItem::RouterTreeItem(const RouterConfig& router)
{
    setRouter(router);
}

RouterTreeItem::~RouterTreeItem() = default;

void RouterTreeItem::setRouter(const RouterConfig& router)
{
    router_ = router;

    setText(0, QString::fromStdU16String(router.name));
    setText(1, QString::fromStdU16String(router.address));
    setText(2, QString::number(router.port));
}

const RouterConfig& RouterTreeItem::router() const
{
    return router_;
}

} // namespace client
