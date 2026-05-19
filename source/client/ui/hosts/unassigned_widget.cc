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
#include <QEvent>
#include <QHeaderView>
#include <QIODevice>
#include <QTreeWidget>

#include "base/logging.h"
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
void UnassignedWidget::showForRouter(qint64 router_id)
{
    router_id_ = router_id;
    // TODO: request unassigned computers list from the router and populate the tree.
    ui->tree_computer->clear();
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
