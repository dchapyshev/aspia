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

#include "client/ui/hosts/local_group_widget.h"

#include <QApplication>
#include <QIODevice>
#include <QMenu>
#include <QMouseEvent>
#include <QUuid>

#include "base/logging.h"
#include "client/local_database.h"

namespace client {

namespace {

const int kColumnName = 0;
const int kColumnAddress = 1;
const int kColumnComment = 2;

class ColumnAction : public QAction
{
public:
    ColumnAction(const QString& text, int index, QObject* parent)
        : QAction(text, parent),
        index_(index)
    {
        setCheckable(true);
    }

    int columnIndex() const { return index_; }

private:
    const int index_;
    Q_DISABLE_COPY_MOVE(ColumnAction)
};

} // namespace

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::LocalGroupWidget(QWidget* parent)
    : ContentWidget(Type::LOCAL_GROUP, parent),
      mime_type_(QString("application/%1").arg(QUuid::createUuid().toString()))
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    ui.tree_computer->viewport()->installEventFilter(this);

    ui.tree_computer->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui.tree_computer->header(), &QHeaderView::customContextMenuRequested,
            this, &LocalGroupWidget::onHeaderContextMenu);

    connect(ui.tree_computer, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        if (!item)
            return;

        Item* computer_item = static_cast<Item*>(item);
        emit sig_doubleClicked(computer_item->computerId());
    });

    connect(ui.tree_computer, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
    {
        qint64 computer_id = -1;

        if (current)
        {
            Item* computer_item = static_cast<Item*>(current);
            computer_id = computer_item->computerId();
        }

        emit sig_currentChanged(computer_id);
    });

    connect(ui.tree_computer, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos)
    {
        qint64 computer_id = 0;
        Item* item = static_cast<Item*>(ui.tree_computer->itemAt(pos));
        if (item)
        {
            ui.tree_computer->setCurrentItem(item);
            computer_id = item->computerId();
        }
        emit sig_contextMenu(computer_id, ui.tree_computer->viewport()->mapToGlobal(pos));
    });
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::~LocalGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::Item* LocalGroupWidget::currentItem()
{
    return static_cast<Item*>(ui.tree_computer->currentItem());
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::showGroup(qint64 group_id)
{
    ui.tree_computer->clear();

    QList<ComputerData> computers = LocalDatabase::instance().computerList(group_id);

    for (const ComputerData& computer : std::as_const(computers))
        new Item(computer, ui.tree_computer);
}

//--------------------------------------------------------------------------------------------------
int LocalGroupWidget::itemCount() const
{
    return ui.tree_computer->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
QByteArray LocalGroupWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_15);

        stream << ui.tree_computer->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_15);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
        ui.tree_computer->header()->restoreState(columns_state);
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui.tree_computer->viewport())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton)
                start_pos_ = mouse_event->pos();
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->buttons() & Qt::LeftButton)
            {
                int distance = (mouse_event->pos() - start_pos_).manhattanLength();
                if (distance > QApplication::startDragDistance())
                {
                    startDrag();
                    return true;
                }
            }
        }
    }

    return ContentWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::startDrag()
{
    Item* computer_item = static_cast<Item*>(ui.tree_computer->itemAt(start_pos_));
    if (computer_item)
    {
        ComputerDrag* drag = new ComputerDrag(this);

        drag->setComputerItem(computer_item, mime_type_);

        QIcon icon = computer_item->icon(0);
        drag->setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag->exec(Qt::MoveAction);
    }
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::onHeaderContextMenu(const QPoint &pos)
{
    QHeaderView* header = ui.tree_computer->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui.tree_computer->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::Item::Item(const ComputerData& computer, QTreeWidget* parent)
    : QTreeWidgetItem(parent),
      computer_id_(computer.id),
      group_id_(computer.group_id),
      computer_name_(computer.name)
{
    setText(kColumnName, computer.name);
    setText(kColumnAddress, computer.address);
    setText(kColumnComment, computer.comment);
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
}

} // namespace client
