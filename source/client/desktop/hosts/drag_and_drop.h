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

#ifndef CLIENT_DESKTOP_HOSTS_DRAG_AND_DROP_H
#define CLIENT_DESKTOP_HOSTS_DRAG_AND_DROP_H

#include <QDrag>
#include <QMimeData>

#include "client/router.h"
#include "client/desktop/hosts/local_group_widget.h"

class SidebarLocalGroup;
class SidebarRouterGroup;

//--------------------------------------------------------------------------------------------------
class LocalGroupMimeData final : public QMimeData
{
public:
    LocalGroupMimeData() = default;
    ~LocalGroupMimeData() final = default;

    void setGroupItem(SidebarLocalGroup* group_item, const QString& mime_type);
    SidebarLocalGroup* groupItem() const { return group_item_; }

private:
    SidebarLocalGroup* group_item_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
class LocalGroupDrag final : public QDrag
{
public:
    explicit LocalGroupDrag(QObject* drag_source = nullptr);

    void setGroupItem(SidebarLocalGroup* group_item, const QString& mime_type);
};

//--------------------------------------------------------------------------------------------------
class LocalHostMimeData final : public QMimeData
{
public:
    LocalHostMimeData() = default;
    ~LocalHostMimeData() final = default;

    void setHostItem(LocalGroupWidget::Item* host_item, const QString& mime_type);
    LocalGroupWidget::Item* hostItem() const { return host_item_; }

private:
    LocalGroupWidget::Item* host_item_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
class LocalHostDrag final : public QDrag
{
public:
    explicit LocalHostDrag(QObject* drag_source = nullptr);

    void setHostItem(LocalGroupWidget::Item* host_item, const QString& mime_type);
};

//--------------------------------------------------------------------------------------------------
class RouterGroupMimeData final : public QMimeData
{
public:
    RouterGroupMimeData() = default;
    ~RouterGroupMimeData() final = default;

    void setGroupItem(SidebarRouterGroup* group_item, const QString& mime_type);
    SidebarRouterGroup* groupItem() const { return group_item_; }

private:
    SidebarRouterGroup* group_item_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
class RouterGroupDrag final : public QDrag
{
public:
    explicit RouterGroupDrag(QObject* drag_source = nullptr);

    void setGroupItem(SidebarRouterGroup* group_item, const QString& mime_type);
};

//--------------------------------------------------------------------------------------------------
class RouterHostMimeData final : public QMimeData
{
public:
    RouterHostMimeData() = default;
    ~RouterHostMimeData() final = default;

    void setHost(qint64 router_id, const Router::Host& host, const QString& mime_type);

    qint64 routerId() const { return router_id_; }
    const Router::Host& host() const { return host_; }

private:
    qint64 router_id_ = 0;
    Router::Host host_;
};

//--------------------------------------------------------------------------------------------------
class RouterHostDrag final : public QDrag
{
public:
    explicit RouterHostDrag(QObject* drag_source = nullptr);

    void setHost(qint64 router_id, const Router::Host& host, const QString& mime_type);
};

#endif // CLIENT_DESKTOP_HOSTS_DRAG_AND_DROP_H
