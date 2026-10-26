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

#include "client/desktop/management/router_access_log_widget.h"

#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QHeaderView>
#include <QIODevice>
#include <QMenu>
#include <QSignalBlocker>
#include <QTimer>

#include "base/auto_qpointer.h"
#include "base/logging.h"
#include "client/router_controller.h"
#include "client/desktop/management/ui_router_access_log_widget.h"
#include "common/desktop/session_type.h"
#include "proto/peer.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

// Upper sanity bound on the router-reported entry count, used only for pagination.
const qint64 kMaxEntryCount = 1000000;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterAccessLogWidget::RouterAccessLogWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_ACCESS_LOG, parent),
      ui(std::make_unique<Ui::RouterAccessLogWidget>())
{
    LOG(TRACE) << "Ctor";
    ui->setupUi(this);

    model_ = new AccessLogListModel(this);
    ui->tree_log->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tree_log->setModel(model_);

    // Turned on again after the model is set, because the view wires the header up to the sort of
    // the model it has and the generated setup ran without one.
    ui->tree_log->setSortingEnabled(true);

    QHeaderView* header = ui->tree_log->header();
    header->resizeSection(static_cast<int>(AccessLogListModel::Column::TIME), 130);
    header->resizeSection(static_cast<int>(AccessLogListModel::Column::CLIENT_COMPUTER_NAME), 160);
    header->resizeSection(static_cast<int>(AccessLogListModel::Column::HOST_COMPUTER_NAME), 160);
    header->setSectionHidden(static_cast<int>(AccessLogListModel::Column::CLIENT_VERSION), true);
    header->setSectionHidden(static_cast<int>(AccessLogListModel::Column::CLIENT_OS), true);

    ui->tree_log->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_log, &QWidget::customContextMenuRequested,
            this, &RouterAccessLogWidget::onLogContextMenu);

    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::customContextMenuRequested,
            this, &RouterAccessLogWidget::onHeaderContextMenu);

    // The largest entry is the largest page the router serves (kMaxAccessLogPageSize).
    ui->combo_page_size->addItem("25", QVariant::fromValue<qint64>(25));
    ui->combo_page_size->addItem("50", QVariant::fromValue<qint64>(50));
    ui->combo_page_size->addItem("100", QVariant::fromValue<qint64>(100));
    ui->combo_page_size->setCurrentIndex(2);
    page_.setPageSize(ui->combo_page_size->currentData().toLongLong());

    ui->button_next->setIconOnRight(true);

    connect(ui->combo_page_size, &QComboBox::currentIndexChanged, this, &RouterAccessLogWidget::onPageSizeChanged);
    connect(ui->combo_page, &QComboBox::currentIndexChanged, this, &RouterAccessLogWidget::onPageChanged);
    connect(ui->button_prev, &QToolButton::clicked, this, &RouterAccessLogWidget::onPrevClicked);
    connect(ui->button_next, &QToolButton::clicked, this, &RouterAccessLogWidget::onNextClicked);

    // The lowest date of an edit stands for an open end of the period.
    const QDate min_date(2000, 1, 1);
    for (QDateEdit* edit : { ui->date_filter_from, ui->date_filter_to })
    {
        edit->setMinimumDate(min_date);
        edit->setSpecialValueText(tr("Any"));
        edit->setDate(min_date);
        connect(edit, &QDateEdit::dateChanged, this, &RouterAccessLogWidget::onFilterChanged);
    }

    ui->combo_filter_session_type->addItem(QIcon(":/img/unknown.svg"), tr("All Session Types"),
                                           QVariant::fromValue<int>(proto::peer::SESSION_TYPE_UNKNOWN));

    const proto::peer::SessionType kSessionTypes[] =
    {
        proto::peer::SESSION_TYPE_DESKTOP,
        proto::peer::SESSION_TYPE_FILE_TRANSFER,
        proto::peer::SESSION_TYPE_SYSTEM_INFO,
        proto::peer::SESSION_TYPE_CHAT,
        proto::peer::SESSION_TYPE_TERMINAL
    };

    for (proto::peer::SessionType session_type : kSessionTypes)
    {
        ui->combo_filter_session_type->addItem(sessionIcon(session_type), sessionName(session_type),
                                               QVariant::fromValue<int>(session_type));
    }

    connect(ui->combo_filter_session_type, &QComboBox::currentIndexChanged,
            this, &RouterAccessLogWidget::onFilterChanged);

    filter_timer_ = new QTimer(this);
    filter_timer_->setSingleShot(true);
    filter_timer_->setInterval(MilliSeconds(300));
    connect(filter_timer_, &QTimer::timeout, this, &RouterAccessLogWidget::onFilterChanged);

    // A character of the text takes up to three bytes of the limit.
    for (QLineEdit* edit : { ui->edit_filter_user, ui->edit_filter_client, ui->edit_filter_host })
    {
        edit->setMaxLength(static_cast<int>(proto::router::kMaxAccessLogQueryLength / 3));
        connect(edit, &QLineEdit::textChanged, filter_timer_, qOverload<>(&QTimer::start));
    }

    connect(ui->button_filter_reset, &QPushButton::clicked, this, &RouterAccessLogWidget::onFilterReset);

    updatePagination();
    updateCountLabel();

    connect(&RouterController::instance(), &RouterController::sig_statusChanged, this,
            [this](qint64 router_id, RouterStatus status)
    {
        if (router_id == router_id_ && status != RouterStatus::ONLINE)
        {
            model_->clear();
            page_.clear();
            updatePagination();
            updateCountLabel();
        }
    });
}

//--------------------------------------------------------------------------------------------------
RouterAccessLogWidget::~RouterAccessLogWidget()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::showRouter(qint64 router_id)
{
    router_id_ = router_id;
    page_.clear();
    max_entry_id_ = 0;

    model_->clear();
    updatePagination();
    updateCountLabel();
    fetchEntries();
}

//--------------------------------------------------------------------------------------------------
bool RouterAccessLogWidget::hasCurrentEntry() const
{
    return model_->entryAt(ui->tree_log->currentIndex().row()) != nullptr;
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::copyCurrentRow()
{
    const int row = ui->tree_log->currentIndex().row();
    if (row < 0)
        return;

    QString result;
    for (int i = 0; i < model_->columnCount(); ++i)
    {
        if (ui->tree_log->header()->isSectionHidden(i))
            continue;

        const QString text = model_->index(row, i).data().toString();
        if (!text.isEmpty())
            result += text + ' ';
    }
    result.chop(1);

    if (result.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(result);
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::copyCurrentColumn(int column)
{
    const int row = ui->tree_log->currentIndex().row();
    if (row < 0 || column < 0 || column >= model_->columnCount())
        return;

    const QString text = model_->index(row, column).data().toString();
    if (text.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterAccessLogWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_log->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
    {
        ui->tree_log->header()->restoreState(columns_state);
        ui->tree_log->header()->setSectionsClickable(true);
        ui->tree_log->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::reload()
{
    max_entry_id_ = 0;
    fetchEntries();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onAccessLogReceived(const proto::router::AccessLogList& list)
{
    if (list.error_code() != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Unable to get the access log:" << list.error_code();
        return;
    }

    const proto::router::AccessLogEntry* current = model_->entryAt(ui->tree_log->currentIndex().row());
    const qint64 current_entry_id = current ? current->entry_id() : 0;

    model_->setEntries(list);
    max_entry_id_ = list.max_entry_id();

    // The page is replaced whole, so the row the user was on has to be found again by its entry.
    const int current_row = model_->rowOf(current_entry_id);
    if (current_row >= 0)
        ui->tree_log->setCurrentIndex(model_->index(current_row, 0));

    const bool page_moved = page_.setTotalCount(qMin<qint64>(list.total_count(), kMaxEntryCount));
    updatePagination();

    // The page the list was fetched for is gone, so nothing would be shown without another fetch.
    if (page_moved)
        fetchEntries();

    updateCountLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onLogContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->tree_log->indexAt(pos);
    if (!index.isValid())
        return;

    ui->tree_log->setCurrentIndex(index);
    emit sig_contextMenu(ui->tree_log->viewport()->mapToGlobal(pos), index.column());
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onHeaderContextMenu(const QPoint& pos)
{
    QPointer<QHeaderView> header = ui->tree_log->header();
    AutoQPointer<QMenu> menu(new QMenu(this));

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(
            model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), i, menu);
        action->setChecked(!header->isSectionHidden(i));
        menu->addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu->exec(header->viewport()->mapToGlobal(pos)));
    if (!action || !header)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onPageSizeChanged(int /* index */)
{
    page_.setPageSize(ui->combo_page_size->currentData().toLongLong());
    page_.setCurrentPage(0);
    max_entry_id_ = 0;
    fetchEntries();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onPageChanged(int index)
{
    if (index < 0)
        return;

    if (index == page_.currentPage())
        return;

    page_.setCurrentPage(index);
    fetchEntries();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onPrevClicked()
{
    if (page_.currentPage() <= 0)
        return;

    page_.setCurrentPage(page_.currentPage() - 1);
    fetchEntries();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onNextClicked()
{
    if (page_.currentPage() >= page_.pageCount() - 1)
        return;

    page_.setCurrentPage(page_.currentPage() + 1);
    fetchEntries();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onFilterChanged()
{
    filter_timer_->stop();
    page_.setCurrentPage(0);
    max_entry_id_ = 0;
    fetchEntries();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::onFilterReset()
{
    clearFilter();
    onFilterChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::fetchEntries()
{
    RouterSession* session = RouterController::session(router_id_);
    if (!session)
        return;

    if (session->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    proto::router::AccessLogListRequest request;
    request.set_offset(page_.offset());
    request.set_count(page_.pageSize());
    request.set_max_entry_id(max_entry_id_);
    *request.mutable_filter() = filter();

    session->listAccessLog(std::move(request), { this, &RouterAccessLogWidget::onAccessLogReceived });
}

//--------------------------------------------------------------------------------------------------
proto::router::AccessLogFilter RouterAccessLogWidget::filter() const
{
    proto::router::AccessLogFilter filter;

    // The period takes whole days of the local time, from the start of the first one to the end of
    // the last one.
    const QDate from = ui->date_filter_from->date();
    if (from != ui->date_filter_from->minimumDate())
        filter.set_time_from(from.startOfDay().toSecsSinceEpoch());

    const QDate to = ui->date_filter_to->date();
    if (to != ui->date_filter_to->minimumDate())
        filter.set_time_to(to.addDays(1).startOfDay().toSecsSinceEpoch());

    filter.set_session_type(static_cast<proto::peer::SessionType>(
        ui->combo_filter_session_type->currentData().toInt()));
    filter.set_user_name(ui->edit_filter_user->text().trimmed().toStdString());
    filter.set_client(ui->edit_filter_client->text().trimmed().toStdString());
    filter.set_host(ui->edit_filter_host->text().trimmed().toStdString());
    return filter;
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::clearFilter()
{
    const QSignalBlocker from_blocker(ui->date_filter_from);
    const QSignalBlocker to_blocker(ui->date_filter_to);
    const QSignalBlocker type_blocker(ui->combo_filter_session_type);
    const QSignalBlocker user_blocker(ui->edit_filter_user);
    const QSignalBlocker client_blocker(ui->edit_filter_client);
    const QSignalBlocker host_blocker(ui->edit_filter_host);

    ui->date_filter_from->setDate(ui->date_filter_from->minimumDate());
    ui->date_filter_to->setDate(ui->date_filter_to->minimumDate());
    ui->combo_filter_session_type->setCurrentIndex(0);
    ui->edit_filter_user->clear();
    ui->edit_filter_client->clear();
    ui->edit_filter_host->clear();
    filter_timer_->stop();
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::updatePagination()
{
    const qint64 total_pages = page_.pageCount();

    QSignalBlocker blocker(ui->combo_page);
    ui->combo_page->clear();
    for (qint64 i = 1; i <= total_pages; ++i)
        ui->combo_page->addItem(QString::number(i));
    ui->combo_page->setCurrentIndex(static_cast<int>(page_.currentPage()));

    ui->combo_page->setEnabled(total_pages > 1);
    ui->button_prev->setEnabled(page_.currentPage() > 0);
    ui->button_next->setEnabled(page_.currentPage() < total_pages - 1);
}

//--------------------------------------------------------------------------------------------------
void RouterAccessLogWidget::updateCountLabel()
{
    ui->label_count->setText(tr("%n entry(s)", "", page_.totalCount()));
}
