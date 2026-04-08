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

#include "client/ui/computers_tab/local_group_widget.h"

#include <QIODevice>
#include <QMenu>

#include "base/logging.h"
#include "client/local_database.h"
#include "client/ui/computers_tab/content_tree_item.h"

namespace client {

namespace {

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
    : ContentWidget(Type::LOCAL_GROUP, parent)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    ui.tree_computer->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui.tree_computer->header(), &QHeaderView::customContextMenuRequested,
            this, &LocalGroupWidget::onHeaderContextMenu);

    connect(ui.tree_computer, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        if (!item)
            return;

        ContentTreeItem* computer_item = static_cast<ContentTreeItem*>(item);
        emit sig_computerDoubleClicked(computer_item->computerId());
    });

    connect(ui.tree_computer, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
    {
        qint64 computer_id = -1;

        if (current)
        {
            LocalComputerItem* computer_item = static_cast<LocalComputerItem*>(current);
            computer_id = computer_item->computerId();
        }

        emit sig_currentComputerChanged(computer_id);
    });
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::~LocalGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
LocalComputerItem* LocalGroupWidget::currentComputer()
{
    return static_cast<LocalComputerItem*>(ui.tree_computer->currentItem());
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::showGroup(qint64 group_id)
{
    ui.tree_computer->clear();

    QList<ComputerData> computers = LocalDatabase::instance().computerList(group_id);

    for (const ComputerData& computer : std::as_const(computers))
        new LocalComputerItem(computer, ui.tree_computer);
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

} // namespace client
