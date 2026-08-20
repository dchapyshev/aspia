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

#include "client/desktop/management/local_group_widget.h"

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
#include "client/desktop/management/drag_and_drop.h"
#include "client/online_checker/online_checker.h"
#include "ui_local_group_widget.h"

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

    model_ = new LocalHostListModel(this);
    ui->tree_host->setModel(model_);

    // Turned on again after the model is set: the view wires the header up to the sort of whatever
    // model it has, and at the time the generated setup ran there was none.
    ui->tree_host->setSortingEnabled(true);

    status_check_label_->setVisible(false);

    ui->tree_host->viewport()->installEventFilter(this);
    ui->tree_host->installEventFilter(this);

    ui->tree_host->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tree_host->header(), &QHeaderView::customContextMenuRequested,
            this, &LocalGroupWidget::onHeaderContextMenu);

    connect(ui->tree_host, &QAbstractItemView::activated, this, [this](const QModelIndex& index)
    {
        if (const LocalHostConfig* host = model_->hostAt(index.row()))
            emit sig_activated(host->id());
    });

    connect(ui->tree_host->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex& /* previous */)
    {
        const LocalHostConfig* host = model_->hostAt(current.row());
        emit sig_currentChanged(host ? host->id() : -1);
    });

    connect(ui->tree_host, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        const QModelIndex index = ui->tree_host->indexAt(pos);
        if (index.isValid())
            ui->tree_host->setCurrentIndex(index);

        const LocalHostConfig* host = model_->hostAt(index.row());
        emit sig_contextMenu(host ? host->id() : 0,
                             ui->tree_host->viewport()->mapToGlobal(pos));
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
const LocalHostConfig* LocalGroupWidget::currentHost() const
{
    return model_->hostAt(ui->tree_host->currentIndex().row());
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::showGroup(qint64 group_id)
{
    current_group_id_ = group_id;

    model_->setHosts(Database::instance().localHostList(group_id));

    updateStatusLabels();

    if (online_check_enabled_)
        startOnlineChecker();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setConnectTime(qint64 entry_id, qint64 connect_time)
{
    model_->setConnectTime(entry_id, connect_time);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setOnlineCheckEnabled(bool enable)
{
    online_check_enabled_ = enable;
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::setCurrentHost(qint64 entry_id)
{
    const int row = model_->rowOf(entry_id);
    if (row < 0)
        return;

    ui->tree_host->setCurrentIndex(model_->index(row, 0));
    ui->tree_host->setFocus();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::refreshItem(qint64 entry_id)
{
    if (model_->rowOf(entry_id) < 0)
        return;

    std::optional<LocalHostConfig> updated = Database::instance().findLocalHost(entry_id);
    if (!updated.has_value())
    {
        removeItem(entry_id);
        return;
    }

    model_->updateHost(*updated);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::removeItem(qint64 entry_id)
{
    const int row = model_->rowOf(entry_id);
    if (row < 0)
        return;

    model_->removeHost(entry_id);

    // The row the user was on is gone, so the one that took its place is picked instead.
    const int count = model_->rowCount();
    if (count > 0)
    {
        ui->tree_host->setCurrentIndex(model_->index(qMin(row, count - 1), 0));
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
    ids.reserve(model_->rowCount());
    for (const LocalHostConfig& host : model_->hosts())
        ids.append(host.id());

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
bool LocalGroupWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tree_host)
    {
        if (event->type() == QEvent::KeyPress)
        {
            switch (static_cast<QKeyEvent*>(event)->key())
            {
                case Qt::Key_Insert:
                    emit sig_addHost();
                    return true;

                case Qt::Key_Delete:
                    emit sig_deleteHost();
                    return true;

                case Qt::Key_F2:
                    emit sig_editHost();
                    return true;

                default:
                    break;
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
        ColumnAction* action = new ColumnAction(
            model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), i, &menu);
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
    model_->setOnlineStatus(entry_id, online);
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
    const QModelIndex index = ui->tree_host->indexAt(start_pos_);
    const LocalHostConfig* host = model_->hostAt(index.row());
    if (!host)
        return;

    LocalHostDrag drag(this);
    drag.setHost(*host, mime_type_);

    const QIcon icon = index.siblingAtColumn(0).data(Qt::DecorationRole).value<QIcon>();
    drag.setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

    drag.exec(Qt::MoveAction);
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::updateStatusLabels()
{
    int child_groups_count = 0;
    if (current_group_id_ >= 0)
        child_groups_count = Database::instance().localGroupList(current_group_id_).size();

    status_groups_label_->setText(tr("%n child group(s)", "", child_groups_count));
    status_hosts_label_->setText(tr("%n child host(s)", "", model_->rowCount()));
}

//--------------------------------------------------------------------------------------------------
void LocalGroupWidget::startOnlineChecker()
{
    clearOnlineStatuses();

    OnlineChecker::HostList hosts = model_->hosts();

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
    model_->clearOnlineStatuses();
}
