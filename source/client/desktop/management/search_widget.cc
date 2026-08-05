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

#include "client/desktop/management/search_widget.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDataStream>
#include <QEvent>
#include <QFontMetrics>
#include <QComboBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <optional>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/database.h"
#include "client/router.h"
#include "common/desktop/icon_text_button.h"
#include "proto/router_constants.h"

namespace {

const int kColumnName    = 0;
const int kColumnAddress = 1;
const int kColumnGroup   = 2;
const int kColumnComment = 3;

// Matches shown at a time, over every source together. Not larger than the page a router
// serves at once, because a page that spans a single source is fetched with one request.
const qint64 kPageSize = proto::router::kMaxHostPageSize;

// Match counts a source can carry while it has not answered yet, or has failed to. Both keep
// it out of the paging arithmetic; only the first one makes the page wait for it.
const qint64 kSourceUnanswered = -1;
const qint64 kSourceFailed     = -2;

//--------------------------------------------------------------------------------------------------
QString buildHighlightedHtml(const QString& text, const QString& query)
{
    QString result;

    int from = 0;
    const int query_len = query.length();

    while (from < text.length())
    {
        int idx = text.indexOf(query, from, Qt::CaseInsensitive);
        if (idx < 0)
        {
            result += text.mid(from).toHtmlEscaped();
            break;
        }

        result += text.mid(from, idx - from).toHtmlEscaped();
        result += "<span style=\"background-color:#ffeb3b;color:#000000\">";
        result += text.mid(idx, query_len).toHtmlEscaped();
        result += "</span>";

        from = idx + query_len;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QString buildGroupPath(qint64 group_id, const QHash<qint64, GroupConfig>& groups)
{
    QStringList parts;
    qint64 current = group_id;

    while (current > 0)
    {
        auto it = groups.find(current);
        if (it == groups.end())
            break;

        parts.prepend(it.value().name());
        current = it.value().parentId();
    }

    // Every group of the address book hangs off its root, so the path starts there whatever the
    // depth. A host that is in no group is in the root itself and shows the root alone. The name
    // is the one the sidebar puts on it.
    parts.prepend(QCoreApplication::translate("Sidebar", "Local"));

    return parts.join(" / ");
}

//--------------------------------------------------------------------------------------------------
SearchResultModel::Row makeLocalRow(const HostConfig& host, const QString& group_path)
{
    SearchResultModel::Row row;
    row.type = SearchResultModel::Type::LOCAL;
    row.host = host;
    row.source = group_path;
    return row;
}

//--------------------------------------------------------------------------------------------------
// A router host is put into a record of the same shape as one of the address book, so a row of
// either kind is shown and connected to the same way. The record is never stored, so it has no id.
SearchResultModel::Row makeRouterRow(qint64 router_id, const Router::Host& host,
                                     const QString& source_label)
{
    QString name = host.display_name;
    if (name.isEmpty())
        name = host.computer_name;

    SearchResultModel::Row row;
    row.type = SearchResultModel::Type::ROUTER;
    row.source = source_label;

    row.host.setRouterId(router_id);
    row.host.setAddress(hostIdToString(host.host_id));
    row.host.setName(name);
    row.host.setUsername(host.user_name);
    row.host.setPassword(host.password);
    row.host.setComment(host.comment);

    return row;
}

} // namespace

class SearchWidget::HighlightDelegate final : public QStyledItemDelegate
{
public:
    explicit HighlightDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        // Nothing
    }

    ~HighlightDelegate() final = default;

    void setQuery(const QString& query) { query_ = query; }

    // QStyledItemDelegate implementation.
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const final
    {
        const QString text = index.data(Qt::DisplayRole).toString();

        if (query_.isEmpty() || !text.contains(query_, Qt::CaseInsensitive))
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QWidget* widget = opt.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();

        // Draw the standard background, focus and icon, but not the text.
        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        QRect text_rect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);

        QString elided = QFontMetrics(opt.font).elidedText(text, opt.textElideMode, text_rect.width());

        QPalette::ColorGroup cg = QPalette::Normal;
        if (!(opt.state & QStyle::State_Enabled))
            cg = QPalette::Disabled;
        else if (!(opt.state & QStyle::State_Active))
            cg = QPalette::Inactive;

        QPalette::ColorRole role = (opt.state & QStyle::State_Selected) ?
            QPalette::HighlightedText : QPalette::Text;
        QColor base_color = opt.palette.color(cg, role);

        QTextOption text_option;
        text_option.setWrapMode(QTextOption::NoWrap);

        QTextDocument doc;
        doc.setDefaultFont(opt.font);
        doc.setDocumentMargin(0);
        doc.setDefaultTextOption(text_option);
        doc.setDefaultStyleSheet(QString("body { color: %1; }").arg(base_color.name(QColor::HexRgb)));
        doc.setHtml(buildHighlightedHtml(elided, query_));
        doc.setTextWidth(text_rect.width());

        const qreal y_offset = (text_rect.height() - doc.size().height()) / 2.0;

        painter->save();
        painter->setClipRect(text_rect);
        painter->translate(text_rect.left(), text_rect.top() + qMax(qreal(0), y_offset));
        doc.documentLayout()->draw(painter, QAbstractTextDocumentLayout::PaintContext());
        painter->restore();
    }

private:
    QString query_;
};

//--------------------------------------------------------------------------------------------------
SearchWidget::SearchWidget(QWidget* parent)
    : ContentWidget(Type::SEARCH, parent),
      status_results_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    model_ = new SearchResultModel(this);

    tree_host_ = new QTreeView(this);
    tree_host_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_host_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_host_->setRootIsDecorated(false);
    tree_host_->setIndentation(0);
    tree_host_->setModel(model_);

    // Turned on after the model is set: the view wires the header up to the sort of whatever model
    // it has, and without one there is nothing to sort.
    tree_host_->setSortingEnabled(true);

    QHeaderView* header = tree_host_->header();
    header->resizeSection(kColumnName, 200);
    header->resizeSection(kColumnAddress, 180);
    header->resizeSection(kColumnGroup, 150);
    header->resizeSection(kColumnComment, 250);

    highlight_delegate_ = new HighlightDelegate(this);
    tree_host_->setItemDelegateForColumn(kColumnName, highlight_delegate_);
    tree_host_->setItemDelegateForColumn(kColumnAddress, highlight_delegate_);
    tree_host_->setItemDelegateForColumn(kColumnGroup, highlight_delegate_);
    tree_host_->setItemDelegateForColumn(kColumnComment, highlight_delegate_);

    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::customContextMenuRequested, this, &SearchWidget::onHeaderContextMenu);

    connect(tree_host_, &QAbstractItemView::activated, this, [this](const QModelIndex& index)
    {
        if (!index.isValid())
            return;

        tree_host_->setCurrentIndex(index);
        emit sig_activated();
    });

    connect(tree_host_->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &SearchWidget::sig_currentChanged);

    connect(tree_host_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        const QModelIndex index = tree_host_->indexAt(pos);
        if (index.isValid())
            tree_host_->setCurrentIndex(index);

        emit sig_contextMenu(tree_host_->viewport()->mapToGlobal(pos));
    });

    // Router searches are issued only after the user stops typing, so a request is not sent on
    // every keystroke. Local results are shown immediately by search().
    router_search_timer_ = new QTimer(this);
    router_search_timer_->setSingleShot(true);
    router_search_timer_->setInterval(MilliSeconds(300));
    connect(router_search_timer_, &QTimer::timeout, this, &SearchWidget::dispatchRouterSearch);

    button_prev_ = new IconTextButton(this);
    button_prev_->setText(tr("Previous"));
    button_prev_->setToolTip(tr("Previous page"));
    button_prev_->setIcon(QIcon(":/img/arrow-left.svg"));

    combo_page_ = new QComboBox(this);

    button_next_ = new IconTextButton(this);
    button_next_->setText(tr("Next"));
    button_next_->setToolTip(tr("Next page"));
    button_next_->setIcon(QIcon(":/img/arrow-right.svg"));
    button_next_->setIconOnRight(true);

    connect(button_prev_, &QToolButton::clicked, this, &SearchWidget::onPrevClicked);
    connect(button_next_, &QToolButton::clicked, this, &SearchWidget::onNextClicked);
    connect(combo_page_, &QComboBox::currentIndexChanged, this, &SearchWidget::onPageChanged);

    QHBoxLayout* pagination_layout = new QHBoxLayout();
    pagination_layout->addWidget(button_prev_);
    pagination_layout->addWidget(combo_page_);
    pagination_layout->addWidget(button_next_);
    pagination_layout->addStretch();

    layout->addWidget(tree_host_);
    layout->addLayout(pagination_layout);

    updatePagination();
}

//--------------------------------------------------------------------------------------------------
SearchWidget::~SearchWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::search(const QString& query)
{
    current_query_ = query;
    highlight_delegate_->setQuery(query);
    model_->clear();

    ++generation_;
    page_model_.clear();
    sources_.clear();
    page_slices_.clear();
    local_matches_.clear();
    local_groups_.clear();

    if (query.isEmpty())
    {
        router_search_timer_->stop();
        updatePagination();
        updateStatusLabels();
        return;
    }

    Database& db = Database::instance();

    const QList<GroupConfig> all_groups = db.allGroups();
    local_groups_.reserve(all_groups.size());
    for (const GroupConfig& group : std::as_const(all_groups))
        local_groups_.insert(group.id(), group);

    local_matches_ = db.searchHosts(query);

    // The local matches are in hand already, so the local part of the first page is shown at
    // once, with the local address book as the only source. The routers are queried after the
    // debounce, not on every keystroke, and the page is rebuilt when they answer.
    Source local;
    local.router_id = 0;
    local.match_count = local_matches_.size();
    sources_.append(local);

    fetchCurrentPage();
    router_search_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::dispatchRouterSearch()
{
    if (current_query_.isEmpty())
        return;

    countSources();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::countSources()
{
    const QString query = current_query_;

    // Any reply still on its way belongs to the source list that is being replaced here.
    const quint64 generation = ++generation_;

    sources_.clear();
    page_slices_.clear();

    // The local address book is the first source and its count is known already.
    Source local;
    local.router_id = 0;
    local.match_count = local_matches_.size();
    sources_.append(local);

    // Then the routers, ordered by id, so the whole result keeps one order however fast each of
    // them answers.
    QList<RouterConfig> routers = Database::instance().routerList();
    std::sort(routers.begin(), routers.end(), [](const RouterConfig& first, const RouterConfig& second)
    {
        return first.routerId() < second.routerId();
    });

    QList<int> pending;

    for (const RouterConfig& config : std::as_const(routers))
    {
        Router* router = Router::instance(config.routerId());
        if (!router || router->status() != Router::Status::ONLINE)
            continue;

        Source source;
        source.router_id = config.routerId();
        source.label = config.displayLabel();
        sources_.append(source);

        pending.append(static_cast<int>(sources_.size()) - 1);
    }

    if (pending.isEmpty())
    {
        // Nothing but the local address book to page over.
        fetchCurrentPage();
        return;
    }

    for (int slot : std::as_const(pending))
    {
        const qint64 router_id = sources_[slot].router_id;
        Router* router = Router::instance(router_id);

        // A single record is asked for: what is wanted here is the size of the whole match set,
        // and the page itself is fetched once every source has reported.
        router->searchHosts(query, 0, 1, this,
            [this, generation, slot, router_id](const Router::HostList& list)
        {
            if (generation != generation_)
                return;

            if (list.error_code != proto::router::kErrorOk)
            {
                // Not fatal for the search as a whole (the other sources still contribute), but
                // without the log a failed router looks exactly like "nothing found" there.
                LOG(ERROR) << "Host search failed on router" << router_id << ":" << list.error_code;
                onSourceCounted(slot, kSourceFailed);
                return;
            }

            onSourceCounted(slot, list.total_count);
        });
    }
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::onSourceCounted(int slot, qint64 match_count)
{
    if (slot < 0 || slot >= sources_.size())
        return;

    sources_[slot].match_count = match_count;

    // The boundaries of a page depend on every count, so a page built from a part of them would be
    // rebuilt with different rows the moment the rest arrives.
    for (const Source& source : std::as_const(sources_))
    {
        if (source.match_count == kSourceUnanswered)
            return;
    }

    fetchCurrentPage();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::fetchCurrentPage()
{
    const qint64 wanted_page = page_model_.currentPage();

    // A source that never answered is left out. Its matches are missing from the result either
    // way, and a hole of unknown size would shift the pages of every source after it.
    page_model_.clear();
    page_model_.setPageSize(kPageSize);

    QList<int> model_to_source;
    for (int i = 0; i < sources_.size(); ++i)
    {
        if (sources_[i].match_count < 0)
            continue;

        page_model_.addSource(sources_[i].match_count);
        model_to_source.append(i);
    }

    page_model_.setCurrentPage(wanted_page);
    page_slices_.clear();

    // A reply still on its way belongs to the page being left, and the slot it would write
    // into is about to mean something else.
    ++generation_;

    const QList<SearchPageModel::Slice> slices = page_model_.currentSlices();
    for (const SearchPageModel::Slice& slice : slices)
    {
        PageSlice page_slice;
        page_slice.source = model_to_source[slice.source];
        page_slice.offset = slice.offset;
        page_slice.count = slice.count;
        page_slices_.append(page_slice);
    }

    updatePagination();

    const QString query = current_query_;
    const quint64 generation = generation_;

    for (int i = 0; i < page_slices_.size(); ++i)
    {
        PageSlice& slice = page_slices_[i];
        const qint64 router_id = sources_[slice.source].router_id;

        if (router_id == 0)
        {
            // The local matches are in memory, so the window is a range of them.
            slice.ready = true;
            continue;
        }

        Router* router = Router::instance(router_id);
        if (!router)
        {
            slice.ready = true;
            continue;
        }

        router->searchHosts(query, slice.offset, slice.count, this,
            [this, generation, i, router_id](const Router::HostList& list)
        {
            if (generation != generation_ || i >= page_slices_.size())
                return;

            if (list.error_code != proto::router::kErrorOk)
                LOG(ERROR) << "Host search failed on router" << router_id << ":" << list.error_code;
            else
                page_slices_[i].router_hosts = list.hosts;

            page_slices_[i].ready = true;
            showCurrentPage();
        });
    }

    showCurrentPage();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::showCurrentPage()
{
    // The rows of a page are shown together. A source that is still in flight would otherwise put
    // its rows after the ones of a source that comes later in the order.
    for (const PageSlice& slice : std::as_const(page_slices_))
    {
        if (!slice.ready)
            return;
    }

    QList<SearchResultModel::Row> rows;

    for (const PageSlice& slice : std::as_const(page_slices_))
    {
        const Source& source = sources_[slice.source];

        if (source.router_id == 0)
        {
            for (qint64 i = slice.offset; i < slice.offset + slice.count; ++i)
            {
                if (i >= local_matches_.size())
                    break;

                const HostConfig& host = local_matches_[i];
                rows.append(makeLocalRow(host, buildGroupPath(host.groupId(), local_groups_)));
            }
        }
        else
        {
            for (const Router::Host& host : std::as_const(slice.router_hosts))
                rows.append(makeRouterRow(source.router_id, host, source.label));
        }
    }

    model_->setRows(rows);

    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::updatePagination()
{
    const qint64 page_count = page_model_.pageCount();
    const qint64 current_page = page_model_.currentPage();

    QSignalBlocker blocker(combo_page_);
    combo_page_->clear();
    for (qint64 i = 1; i <= page_count; ++i)
        combo_page_->addItem(QString::number(i));
    combo_page_->setCurrentIndex(static_cast<int>(current_page));

    combo_page_->setEnabled(page_count > 1);
    button_prev_->setEnabled(current_page > 0);
    button_next_->setEnabled(current_page < page_count - 1);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::onPageChanged(int index)
{
    if (index < 0 || index == page_model_.currentPage())
        return;

    page_model_.setCurrentPage(index);
    fetchCurrentPage();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::onPrevClicked()
{
    if (page_model_.currentPage() <= 0)
        return;

    page_model_.setCurrentPage(page_model_.currentPage() - 1);
    fetchCurrentPage();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::onNextClicked()
{
    if (page_model_.currentPage() >= page_model_.pageCount() - 1)
        return;

    page_model_.setCurrentPage(page_model_.currentPage() + 1);
    fetchCurrentPage();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::clear()
{
    router_search_timer_->stop();
    current_query_.clear();
    highlight_delegate_->setQuery(QString());
    model_->clear();

    ++generation_;
    page_model_.clear();
    sources_.clear();
    page_slices_.clear();
    local_matches_.clear();
    local_groups_.clear();

    updatePagination();
    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
const SearchResultModel::Row* SearchWidget::currentRow() const
{
    return model_->rowAt(tree_host_->currentIndex().row());
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::setCurrentHost(qint64 entry_id)
{
    const int row = model_->rowOfEntry(entry_id);
    if (row < 0)
        return;

    tree_host_->setCurrentIndex(model_->index(row, 0));
    tree_host_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::refreshItem(qint64 entry_id)
{
    if (model_->rowOfEntry(entry_id) < 0)
        return;

    std::optional<HostConfig> updated = Database::instance().findHost(entry_id);
    if (!updated.has_value())
    {
        removeItem(entry_id);
        return;
    }

    QHash<qint64, GroupConfig> groups;
    const QList<GroupConfig> all_groups = Database::instance().allGroups();
    groups.reserve(all_groups.size());
    for (const GroupConfig& group : std::as_const(all_groups))
        groups.insert(group.id(), group);

    model_->updateEntry(*updated, buildGroupPath(updated->groupId(), groups));
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::removeItem(qint64 entry_id)
{
    const int row = model_->rowOfEntry(entry_id);
    if (row < 0)
        return;

    model_->removeEntry(entry_id);

    // The row the user was on is gone, so the one that took its place is picked instead.
    const int count = model_->rowCount();
    if (count > 0)
    {
        tree_host_->setCurrentIndex(model_->index(qMin(row, count - 1), 0));
        tree_host_->setFocus();
    }

    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
QByteArray SearchWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << tree_host_->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
        tree_host_->header()->restoreState(columns_state);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabels();

    statusbar->addWidget(status_results_label_);
    status_results_label_->show();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_results_label_);
    status_results_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = tree_host_->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        const QString name = model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();

        ColumnAction* action = new ColumnAction(name, i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::updateStatusLabels()
{
    // The count is of the whole result and not of the page: the page is what the user can
    // see, the count is what they can reach.
    status_results_label_->setText(
        tr("%n result(s)", "", static_cast<int>(page_model_.totalCount())));
}
