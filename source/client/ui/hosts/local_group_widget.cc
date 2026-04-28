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
#include <QDateTime>
#include <QIODevice>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QStatusBar>
#include <QTimer>
#include <QUuid>

#include "base/logging.h"
#include "client/database.h"

namespace client {

namespace {

const int kColumnName = 0;
const int kColumnAddress = 1;
const int kColumnComment = 2;
const int kColumnCreated = 3;
const int kColumnModified = 4;
const int kColumnConnect = 5;
const int kColumnStatus = 6;

//--------------------------------------------------------------------------------------------------
QString formatTimestamp(qint64 unix_seconds)
{
    if (unix_seconds <= 0)
        return QString();

    return QLocale::system().toString(
        QDateTime::fromSecsSinceEpoch(unix_seconds), QLocale::ShortFormat);
}

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
      mime_type_(QString("application/%1").arg(QUuid::createUuid().toString())),
      check_animation_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    ui.tree_computer->viewport()->installEventFilter(this);

    ui.tree_computer->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(check_animation_timer_, &QTimer::timeout, this, [this]()
    {
        if (!status_check_label_)
            return;

        if (check_animation_index_ > 3)
            check_animation_index_ = 0;

        QString dots;
        switch (check_animation_index_)
        {
            case 0: dots = "   "; break;
            case 1: dots = ".  "; break;
            case 2: dots = ".. "; break;
            case 3: dots = "..."; break;
            default: break;
        }

        status_check_label_->setText(tr("Status update") + dots);
        ++check_animation_index_;
    });

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
    stopOnlineChecker();
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::Item* LocalGroupWidget::currentItem()
{
    return static_cast<Item*>(ui.tree_computer->currentItem());
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::showGroup(qint64 group_id)
{
    current_group_id_ = group_id;

    ui.tree_computer->clear();

    QList<ComputerConfig> computers = Database::instance().computerList(group_id);

    for (const ComputerConfig& computer : std::as_const(computers))
        new Item(computer, ui.tree_computer);

    updateStatusLabels();

    if (online_check_enabled_)
        startOnlineChecker();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setOnlineCheckEnabled(bool enable)
{
    online_check_enabled_ = enable;
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setConnectTime(qint64 computer_id, qint64 connect_time)
{
    const int count = ui.tree_computer->topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        Item* item = static_cast<Item*>(ui.tree_computer->topLevelItem(i));
        if (item->computerId() == computer_id)
        {
            item->setConnectTime(connect_time);
            break;
        }
    }
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
void LocalGroupWidget::reload()
{
    startOnlineChecker();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::attach(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    if (!status_groups_label_)
        status_groups_label_ = new QLabel(this);
    if (!status_computers_label_)
        status_computers_label_ = new QLabel(this);
    if (!status_check_label_)
        status_check_label_ = new QLabel(this);

    updateStatusLabels();

    statusbar->addWidget(status_groups_label_);
    statusbar->addWidget(status_computers_label_);
    statusbar->addWidget(status_check_label_);

    status_groups_label_->show();
    status_computers_label_->show();
    status_check_label_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::detach(QStatusBar* statusbar)
{
    stopOnlineChecker();

    if (!statusbar)
        return;

    if (status_groups_label_)
    {
        statusbar->removeWidget(status_groups_label_);
        status_groups_label_->setParent(this);
    }

    if (status_computers_label_)
    {
        statusbar->removeWidget(status_computers_label_);
        status_computers_label_->setParent(this);
    }

    if (status_check_label_)
    {
        statusbar->removeWidget(status_check_label_);
        status_check_label_->setParent(this);
    }
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
void LocalGroupWidget::onOnlineCheckerResult(qint64 computer_id, bool online)
{
    for (int i = 0; i < ui.tree_computer->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(ui.tree_computer->topLevelItem(i));
        if (item->computerId() == computer_id)
        {
            item->setOnlineStatus(online);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::onOnlineCheckerFinished()
{
    LOG(INFO) << "Online checker finished";

    if (online_checker_)
        online_checker_->deleteLater();

    setReloadAnimation(false);
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
void LocalGroupWidget::updateStatusLabels()
{
    int child_groups_count = 0;
    if (current_group_id_ >= 0)
        child_groups_count = Database::instance().groupList(current_group_id_).size();

    if (status_groups_label_)
        status_groups_label_->setText(tr("%n child group(s)", "", child_groups_count));

    if (status_computers_label_)
    {
        status_computers_label_->setText(
            tr("%n child computer(s)", "", ui.tree_computer->topLevelItemCount()));
    }
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::startOnlineChecker()
{
    stopOnlineChecker();

    OnlineChecker::ComputerList computers;

    for (int i = 0; i < ui.tree_computer->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(ui.tree_computer->topLevelItem(i));
        if (item)
            computers.emplace_back(item->computer());
    }

    if (computers.isEmpty())
    {
        LOG(INFO) << "No computers to check";
        return;
    }

    LOG(INFO) << "Start online checker for" << computers.size() << "computer(s)";

    online_checker_ = new OnlineChecker(this);

    connect(online_checker_, &OnlineChecker::sig_checkerResult,
            this, &LocalGroupWidget::onOnlineCheckerResult);
    connect(online_checker_, &OnlineChecker::sig_checkerFinished,
            this, &LocalGroupWidget::onOnlineCheckerFinished);

    online_checker_->start(computers);

    setReloadAnimation(true);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::stopOnlineChecker()
{
    if (online_checker_)
    {
        LOG(INFO) << "Stop online checker";
        online_checker_->disconnect(this);
        online_checker_->deleteLater();
    }

    clearOnlineStatuses();
    setReloadAnimation(false);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::clearOnlineStatuses()
{
    const int count = ui.tree_computer->topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        Item* item = static_cast<Item*>(ui.tree_computer->topLevelItem(i));
        item->clearOnlineStatus();
    }
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setReloadAnimation(bool enable)
{
    if (status_check_label_)
        status_check_label_->setVisible(enable);

    if (enable)
    {
        check_animation_timer_->start(std::chrono::milliseconds(500));
    }
    else
    {
        check_animation_timer_->stop();
        check_animation_index_ = 0;
        if (status_check_label_)
            status_check_label_->setText(QString());
    }
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::Item::Item(const ComputerConfig& computer, QTreeWidget* parent)
    : QTreeWidgetItem(parent),
      computer_(computer)
{
    setText(kColumnName, computer.name);
    setText(kColumnAddress, computer.address);
    setText(kColumnComment, computer.comment);
    setText(kColumnCreated, formatTimestamp(computer.create_time));
    setText(kColumnModified, formatTimestamp(computer.modify_time));
    setText(kColumnConnect, formatTimestamp(computer.connect_time));
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::Item::setConnectTime(qint64 connect_time)
{
    computer_.connect_time = connect_time;
    setText(kColumnConnect, formatTimestamp(connect_time));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::Item::setOnlineStatus(bool online)
{
    setText(kColumnStatus, online ? tr("Online") : tr("Offline"));
    setIcon(kColumnName, QIcon(online ? ":/img/computer-online.svg"
                                      : ":/img/computer-offline.svg"));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::Item::clearOnlineStatus()
{
    setText(kColumnStatus, QString());
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupWidget::Item::operator<(const QTreeWidgetItem& other) const
{
    const int column = treeWidget() ? treeWidget()->sortColumn() : 0;

    if (column == kColumnCreated || column == kColumnModified || column == kColumnConnect)
    {
        const Item* other_item = dynamic_cast<const Item*>(&other);
        if (other_item)
        {
            if (column == kColumnCreated)
                return computer_.create_time < other_item->computer_.create_time;
            if (column == kColumnModified)
                return computer_.modify_time < other_item->computer_.modify_time;
            return computer_.connect_time < other_item->computer_.connect_time;
        }
    }

    return QTreeWidgetItem::operator<(other);
}

} // namespace client
