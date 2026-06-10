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

#include "client/ui/hosts/drag_and_drop.h"

//--------------------------------------------------------------------------------------------------
void LocalGroupMimeData::setGroupItem(SidebarLocalGroup* group_item, const QString& mime_type)
{
    group_item_ = group_item;
    setData(mime_type, QByteArray());
}

//--------------------------------------------------------------------------------------------------
LocalGroupDrag::LocalGroupDrag(QObject* drag_source)
    : QDrag(drag_source)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void LocalGroupDrag::setGroupItem(SidebarLocalGroup* group_item, const QString& mime_type)
{
    LocalGroupMimeData* mime_data = new LocalGroupMimeData();
    mime_data->setGroupItem(group_item, mime_type);
    setMimeData(mime_data);
}

//--------------------------------------------------------------------------------------------------
void LocalHostMimeData::setHostItem(LocalGroupWidget::Item* host_item, const QString& mime_type)
{
    host_item_ = host_item;
    setData(mime_type, QByteArray());
}

//--------------------------------------------------------------------------------------------------
LocalHostDrag::LocalHostDrag(QObject* drag_source)
    : QDrag(drag_source)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void LocalHostDrag::setHostItem(LocalGroupWidget::Item* host_item, const QString& mime_type)
{
    LocalHostMimeData* mime_data = new LocalHostMimeData();
    mime_data->setHostItem(host_item, mime_type);
    setMimeData(mime_data);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupMimeData::setGroupItem(SidebarRouterGroup* group_item, const QString& mime_type)
{
    group_item_ = group_item;
    setData(mime_type, QByteArray());
}

//--------------------------------------------------------------------------------------------------
RouterGroupDrag::RouterGroupDrag(QObject* drag_source)
    : QDrag(drag_source)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void RouterGroupDrag::setGroupItem(SidebarRouterGroup* group_item, const QString& mime_type)
{
    RouterGroupMimeData* mime_data = new RouterGroupMimeData();
    mime_data->setGroupItem(group_item, mime_type);
    setMimeData(mime_data);
}

//--------------------------------------------------------------------------------------------------
void RouterHostMimeData::setHost(qint64 router_id, const Router::Host& host,
                                 const QString& mime_type)
{
    router_id_ = router_id;
    host_ = host;
    setData(mime_type, QByteArray());
}

//--------------------------------------------------------------------------------------------------
RouterHostDrag::RouterHostDrag(QObject* drag_source)
    : QDrag(drag_source)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void RouterHostDrag::setHost(qint64 router_id, const Router::Host& host, const QString& mime_type)
{
    RouterHostMimeData* mime_data = new RouterHostMimeData();
    mime_data->setHost(router_id, host, mime_type);
    setMimeData(mime_data);
}
