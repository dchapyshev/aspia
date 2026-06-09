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
#include <QEvent>
#include <QIODevice>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QStatusBar>
#include <QUuid>

#include <optional>

#include "base/logging.h"
#include "client/database.h"
#include "client/online_checker/online_checker.h"
#include "ui_local_group_widget.h"

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
      ui(std::make_unique<Ui::LocalGroupWidget>()),
      mime_type_(QString("application/%1").arg(QUuid::createUuid().toString())),
      status_groups_label_(new QLabel(this)),
      status_hosts_label_(new QLabel(this)),
      status_check_label_(new QLabel(tr("Status update..."), this)),
      online_checker_(new OnlineChecker(this))
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    status_check_label_->setVisible(false);

    ui->tree_host->viewport()->installEventFilter(this);
    ui->tree_host->installEventFilter(this);

    ui->tree_host->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tree_host->header(), &QHeaderView::customContextMenuRequested,
            this, &LocalGroupWidget::onHeaderContextMenu);

    connect(ui->tree_host, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        if (!item)
            return;

        Item* host_item = static_cast<Item*>(item);
        emit sig_activated(host_item->entryId());
    });

    connect(ui->tree_host, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
    {
        qint64 entry_id = -1;

        if (current)
        {
            Item* host_item = static_cast<Item*>(current);
            entry_id = host_item->entryId();
        }

        emit sig_currentChanged(entry_id);
    });

    connect(ui->tree_host, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos)
    {
        qint64 entry_id = 0;
        Item* item = static_cast<Item*>(ui->tree_host->itemAt(pos));
        if (item)
        {
            ui->tree_host->setCurrentItem(item);
            entry_id = item->entryId();
        }
        emit sig_contextMenu(entry_id, ui->tree_host->viewport()->mapToGlobal(pos));
    });

    connect(online_checker_, &OnlineChecker::sig_checkerResult,
            this, &LocalGroupWidget::onOnlineCheckerResult);
    connect(online_checker_, &OnlineChecker::sig_checkerFinished,
            this, &LocalGroupWidget::onOnlineCheckerFinished);
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::~LocalGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::Item* LocalGroupWidget::currentItem()
{
    return static_cast<Item*>(ui->tree_host->currentItem());
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::showGroup(qint64 group_id)
{
    current_group_id_ = group_id;

    ui->tree_host->clear();

    QList<HostConfig> hosts = Database::instance().hostList(group_id);

    for (const HostConfig& host : std::as_const(hosts))
        new Item(host, ui->tree_host);

    updateStatusLabels();

    if (online_check_enabled_)
        startOnlineChecker();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setConnectTime(qint64 entry_id, qint64 connect_time)
{
    const int count = ui->tree_host->topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        Item* item = static_cast<Item*>(ui->tree_host->topLevelItem(i));
        if (item->entryId() == entry_id)
        {
            item->setConnectTime(connect_time);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setOnlineCheckEnabled(bool enable)
{
    online_check_enabled_ = enable;
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setCurrentHost(qint64 entry_id)
{
    Item* item = findItemByEntryId(entry_id);
    if (!item)
        return;

    ui->tree_host->setCurrentItem(item);
    ui->tree_host->setFocus();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::refreshItem(qint64 entry_id)
{
    Item* item = findItemByEntryId(entry_id);
    if (!item)
        return;

    std::optional<HostConfig> updated = Database::instance().findHost(entry_id);
    if (!updated.has_value())
    {
        removeItem(entry_id);
        return;
    }

    item->updateFrom(*updated);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::removeItem(qint64 entry_id)
{
    Item* item = findItemByEntryId(entry_id);
    if (!item)
        return;

    int row = ui->tree_host->indexOfTopLevelItem(item);
    delete ui->tree_host->takeTopLevelItem(row);

    int count = ui->tree_host->topLevelItemCount();
    if (count > 0)
    {
        int next_row = qMin(row, count - 1);
        ui->tree_host->setCurrentItem(ui->tree_host->topLevelItem(next_row));
        ui->tree_host->setFocus();
    }

    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
QByteArray LocalGroupWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_host->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
        ui->tree_host->header()->restoreState(columns_state);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::reload()
{
    QList<qint64> ids;
    ids.reserve(ui->tree_host->topLevelItemCount());
    for (int i = 0; i < ui->tree_host->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(ui->tree_host->topLevelItem(i));
        if (item)
            ids.append(item->entryId());
    }
    online_checker_->invalidate(ids);

    startOnlineChecker();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabels();

    statusbar->addWidget(status_groups_label_);
    statusbar->addWidget(status_hosts_label_);
    statusbar->addWidget(status_check_label_);

    status_groups_label_->show();
    status_hosts_label_->show();
    status_check_label_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_groups_label_);
    status_groups_label_->setParent(this);

    statusbar->removeWidget(status_hosts_label_);
    status_hosts_label_->setParent(this);

    statusbar->removeWidget(status_check_label_);
    status_check_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tree_host)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_Insert)
            {
                emit sig_addHost();
                return true;
            }
        }
    }
    else if (watched == ui->tree_host->viewport())
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
    QHeaderView* header = ui->tree_host->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui->tree_host->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::onOnlineCheckerResult(qint64 entry_id, bool online)
{
    for (int i = 0; i < ui->tree_host->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(ui->tree_host->topLevelItem(i));
        if (item->entryId() == entry_id)
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
    status_check_label_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::startDrag()
{
    Item* host_item = static_cast<Item*>(ui->tree_host->itemAt(start_pos_));
    if (!host_item)
        return;

    HostDrag drag(this);
    drag.setHostItem(host_item, mime_type_);

    const QIcon icon = host_item->icon(0);
    drag.setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

    drag.exec(Qt::MoveAction);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::updateStatusLabels()
{
    int child_groups_count = 0;
    if (current_group_id_ >= 0)
        child_groups_count = Database::instance().groupList(current_group_id_).size();

    status_groups_label_->setText(tr("%n child group(s)", "", child_groups_count));
    status_hosts_label_->setText(
        tr("%n child host(s)", "", ui->tree_host->topLevelItemCount()));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::startOnlineChecker()
{
    clearOnlineStatuses();

    OnlineChecker::HostList hosts;

    for (int i = 0; i < ui->tree_host->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(ui->tree_host->topLevelItem(i));
        if (item)
            hosts.emplace_back(item->host());
    }

    if (hosts.isEmpty())
    {
        LOG(INFO) << "No hosts to check";
        status_check_label_->setVisible(false);
        return;
    }

    LOG(INFO) << "Start online checker for" << hosts.size() << "host(s)";
    online_checker_->start(hosts);
    status_check_label_->setVisible(true);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::clearOnlineStatuses()
{
    const int count = ui->tree_host->topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        Item* item = static_cast<Item*>(ui->tree_host->topLevelItem(i));
        item->clearOnlineStatus();
    }
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::Item* LocalGroupWidget::findItemByEntryId(qint64 entry_id) const
{
    const int count = ui->tree_host->topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        Item* item = static_cast<Item*>(ui->tree_host->topLevelItem(i));
        if (item->entryId() == entry_id)
            return item;
    }
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
LocalGroupWidget::Item::Item(const HostConfig& host, QTreeWidget* parent)
    : QTreeWidgetItem(parent)
{
    updateFrom(host);
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::Item::setConnectTime(qint64 connect_time)
{
    host_.setConnectTime(connect_time);
    setText(kColumnConnect, formatTimestamp(connect_time));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::Item::setOnlineStatus(bool online)
{
    setText(kColumnStatus, online ? tr("Online") : tr("Offline"));
    setIcon(kColumnName, QIcon(online ? ":/img/computer-online.svg" : ":/img/computer-offline.svg"));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::Item::clearOnlineStatus()
{
    setText(kColumnStatus, QString());
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::Item::updateFrom(const HostConfig& host)
{
    host_ = host;

    QString single_line_comment = host.comment();
    single_line_comment.replace('\n', ' ').replace('\r', ' ');

    setText(kColumnName, host.name());
    setText(kColumnAddress, host.address());
    setText(kColumnComment, single_line_comment);
    setToolTip(kColumnComment, host.comment());
    setText(kColumnCreated, formatTimestamp(host.createTime()));
    setText(kColumnModified, formatTimestamp(host.modifyTime()));
    setText(kColumnConnect, formatTimestamp(host.connectTime()));
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
                return host_.createTime() < other_item->host_.createTime();
            if (column == kColumnModified)
                return host_.modifyTime() < other_item->host_.modifyTime();
            return host_.connectTime() < other_item->host_.connectTime();
        }
    }

    return QTreeWidgetItem::operator<(other);
}
