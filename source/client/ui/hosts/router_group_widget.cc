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

#include "client/ui/hosts/router_group_widget.h"

#include <QEvent>

#include "base/logging.h"
#include "client/router.h"
#include "proto/router_client.h"
#include "ui_router_group_widget.h"

namespace {

class WorkspaceHostTreeItem final : public QTreeWidgetItem
{
public:
    explicit WorkspaceHostTreeItem(const proto::router::Host& host)
    {
        setIcon(0, QIcon(":/img/computer.svg"));
        updateItem(host);
    }

    void updateItem(const proto::router::Host& updated_host)
    {
        host_id = updated_host.host_id();

        QString name = QString::fromStdString(updated_host.display_name());
        if (name.isEmpty())
            name = QString::fromStdString(updated_host.computer_name());

        setText(0, name);
        setText(1, QString::number(host_id));
        // TODO: decrypt updated_host.comment() with the workspace group key.
    }

    HostId host_id = kInvalidHostId;

private:
    Q_DISABLE_COPY_MOVE(WorkspaceHostTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::RouterGroupWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_GROUP, parent),
      ui(std::make_unique<Ui::RouterGroupWidget>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);
}

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::~RouterGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::showGroup(qint64 router_id, qint64 workspace_id)
{
    if (router_id_ != router_id)
    {
        // Move the sig_hostsChanged subscription to the router whose workspace is now displayed.
        if (Router* prev = Router::instance(router_id_))
            disconnect(prev, &Router::sig_hostsChanged, this, nullptr);

        if (Router* curr = Router::instance(router_id))
            connect(curr, &Router::sig_hostsChanged, this, &RouterGroupWidget::fetchHosts);
    }

    router_id_ = router_id;
    workspace_id_ = workspace_id;

    // Clear the tree before showing a different workspace to avoid briefly displaying stale
    // entries from the previous one.
    ui->tree_computer->clear();
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterGroupWidget::saveState()
{
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::restoreState(const QByteArray& /* state */)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::reload()
{
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onHostListReceived(const proto::router::HostList& list)
{
    // The router echoes workspace_id/group_id; ignore responses for other workspaces (e.g. an
    // in-flight request issued before the user switched workspaces).
    if (list.workspace_id() != workspace_id_)
        return;

    auto has_with_id = [](const proto::router::HostList& list, HostId host_id)
    {
        for (int i = 0; i < list.host_size(); ++i)
        {
            if (list.host(i).host_id() == host_id)
                return true;
        }
        return false;
    };

    // Remove from the UI all hosts that are not in the list.
    for (int i = ui->tree_computer->topLevelItemCount() - 1; i >= 0; --i)
    {
        WorkspaceHostTreeItem* item =
            static_cast<WorkspaceHostTreeItem*>(ui->tree_computer->topLevelItem(i));

        if (!has_with_id(list, item->host_id))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < list.host_size(); ++i)
    {
        const proto::router::Host& host = list.host(i);
        bool found = false;

        for (int j = 0; j < ui->tree_computer->topLevelItemCount(); ++j)
        {
            WorkspaceHostTreeItem* item =
                static_cast<WorkspaceHostTreeItem*>(ui->tree_computer->topLevelItem(j));
            if (item->host_id == host.host_id())
            {
                item->updateItem(host);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_computer->addTopLevelItem(new WorkspaceHostTreeItem(host));
    }
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::fetchHosts()
{
    if (router_id_ == 0)
        return;

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_FILTERED);
    request.set_workspace_id(workspace_id_);
    router->listHosts(std::move(request), this, &RouterGroupWidget::onHostListReceived);
}
