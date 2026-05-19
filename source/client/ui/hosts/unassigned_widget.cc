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

#include "client/ui/hosts/unassigned_widget.h"

#include <QDataStream>
#include <QDateTime>
#include <QEvent>
#include <QHeaderView>
#include <QIODevice>
#include <QLocale>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "base/logging.h"
#include "client/ui/hosts/router_widget.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "ui_unassigned_widget.h"

//--------------------------------------------------------------------------------------------------
UnassignedWidget::UnassignedWidget(QWidget* parent)
    : ContentWidget(Type::UNASSIGNED, parent),
      ui(std::make_unique<Ui::UnassignedWidget>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);
}

//--------------------------------------------------------------------------------------------------
UnassignedWidget::~UnassignedWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UnassignedWidget::showForRouter(RouterWidget* router_widget)
{
    ui->tree_computer->clear();

    if (router_widget_)
    {
        disconnect(router_widget_, &RouterWidget::sig_computerListReceived,
                   this, &UnassignedWidget::onListReceived);
    }

    router_widget_ = router_widget;
    if (!router_widget_)
        return;

    connect(router_widget_, &RouterWidget::sig_computerListReceived,
            this, &UnassignedWidget::onListReceived);
    router_widget_->onUpdateComputerList(0, 0);
}

//--------------------------------------------------------------------------------------------------
void UnassignedWidget::onListReceived(const proto::router::ComputerList& list)
{
    // We only care about the unassigned bucket here. Other workspace lists may travel through
    // the same signal once other widgets start using it.
    if (list.workspace_id() != 0 || list.group_id() != 0)
        return;

    ui->tree_computer->clear();

    if (list.error_code() != proto::router::kErrorOk)
    {
        LOG(WARNING) << "Computer list error:" << list.error_code();
        return;
    }

    for (int i = 0; i < list.computer_size(); ++i)
    {
        const proto::router::Computer& computer = list.computer(i);

        QTreeWidgetItem* item = new QTreeWidgetItem(ui->tree_computer);
        item->setText(0, QString::number(computer.host_id()));
        item->setText(1, QString::fromStdString(computer.computer_name()));
        item->setText(2, QString::fromStdString(computer.address()));

        if (computer.last_connect() > 0)
        {
            QDateTime time = QDateTime::fromSecsSinceEpoch(computer.last_connect());
            item->setText(3, QLocale::system().toString(time, QLocale::ShortFormat));
        }

        item->setText(4, computer.online() ? tr("Online") : tr("Offline"));
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray UnassignedWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_computer->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void UnassignedWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
    {
        ui->tree_computer->header()->restoreState(columns_state);
        ui->tree_computer->header()->setSectionsClickable(true);
        ui->tree_computer->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void UnassignedWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
}
