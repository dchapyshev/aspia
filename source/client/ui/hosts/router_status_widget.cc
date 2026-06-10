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

#include "client/ui/hosts/router_status_widget.h"

#include <QDataStream>
#include <QEvent>
#include <QIcon>
#include <QTreeWidget>

#include "base/logging.h"
#include "ui_router_status_widget.h"

namespace {

enum Column
{
    COLUMN_TIME = 0,
    COLUMN_EVENT
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterStatusWidget::RouterStatusWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER, parent),
      ui(std::make_unique<Ui::RouterStatusWidget>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);
}

//--------------------------------------------------------------------------------------------------
RouterStatusWidget::~RouterStatusWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::showRouter(qint64 router_id, const QList<Event>& events)
{
    router_id_ = router_id;

    ui->tree_events->clear();
    for (const Event& event : events)
        addEvent(event);

    ui->tree_events->scrollToBottom();
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterStatusWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_events->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
    {
        ui->tree_events->header()->restoreState(columns_state);
        ui->tree_events->header()->setSectionsClickable(true);
        ui->tree_events->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::onEvent(qint64 router_id, const Event& event)
{
    if (router_id != router_id_)
        return;

    addEvent(event);
    ui->tree_events->scrollToBottom();
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::addEvent(const Event& event)
{
    QString icon_path;
    switch (event.severity)
    {
        case Event::Severity::WARNING:  icon_path = ":/img/box-important.svg";   break;
        case Event::Severity::CRITICAL: icon_path = ":/img/high-importance.svg"; break;
        case Event::Severity::INFO:     icon_path = ":/img/info.svg";            break;
    }

    QTreeWidgetItem* item = new QTreeWidgetItem(ui->tree_events);
    item->setText(COLUMN_TIME, event.time.toString("yyyy-MM-dd HH:mm:ss"));
    item->setText(COLUMN_EVENT, event.text);
    item->setIcon(COLUMN_TIME, QIcon(icon_path));
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
}
