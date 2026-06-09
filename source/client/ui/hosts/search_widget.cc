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

#include "client/ui/hosts/search_widget.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDataStream>
#include <QEvent>
#include <QFontMetrics>
#include <QHash>
#include <QHeaderView>
#include <QIODevice>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <optional>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/database.h"
#include "client/router.h"

namespace {

const int kColumnName    = 0;
const int kColumnAddress = 1;
const int kColumnGroup   = 2;
const int kColumnComment = 3;

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

    return parts.join(" / ");
}

} // namespace

//--------------------------------------------------------------------------------------------------
SearchWidget::LocalItem::LocalItem(const HostConfig& host, const QString& group_path,
                                   QTreeWidget* parent)
    : Item(Type::LOCAL, parent)
{
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
    updateFrom(host, group_path);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::LocalItem::updateFrom(const HostConfig& host, const QString& group_path)
{
    host_ = host;

    QString single_line_comment = host.comment();
    single_line_comment.replace('\n', ' ').replace('\r', ' ');

    setText(kColumnName, host.name());
    setText(kColumnAddress, host.address());
    setText(kColumnGroup, group_path);
    setToolTip(kColumnGroup, group_path);
    setText(kColumnComment, single_line_comment);
    setToolTip(kColumnComment, host.comment());
}

//--------------------------------------------------------------------------------------------------
SearchWidget::RouterItem::RouterItem(qint64 router_id, const Router::Host& host,
                                     const QString& source_label, QTreeWidget* parent)
    : Item(Type::ROUTER, parent)
{
    setIcon(kColumnName, QIcon(":/img/computer.svg"));

    QString name = host.display_name;
    if (name.isEmpty())
        name = host.computer_name;

    host_.setRouterId(router_id);
    host_.setAddress(hostIdToString(host.host_id));
    host_.setName(name);
    host_.setUsername(host.user_name);
    host_.setPassword(host.password);
    host_.setComment(host.comment);

    QString single_line_comment = host.comment;
    single_line_comment.replace('\n', ' ').replace('\r', ' ');

    setText(kColumnName, name);
    setText(kColumnAddress, host_.address());
    setText(kColumnGroup, source_label);
    setToolTip(kColumnGroup, source_label);
    setText(kColumnComment, single_line_comment);
    setToolTip(kColumnComment, host.comment);
}

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

    tree_host_ = new QTreeWidget(this);
    tree_host_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_host_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_host_->setIndentation(0);
    tree_host_->setSortingEnabled(true);
    tree_host_->setColumnCount(4);

    QStringList headers;
    headers << tr("Name") << tr("Address / ID") << tr("Group") << tr("Comment");
    tree_host_->setHeaderLabels(headers);

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

    connect(tree_host_, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        if (!item)
            return;

        tree_host_->setCurrentItem(item);
        emit sig_activated();
    });

    connect(tree_host_, &QTreeWidget::currentItemChanged, this, &SearchWidget::sig_currentChanged);

    connect(tree_host_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        Item* item = static_cast<Item*>(tree_host_->itemAt(pos));
        if (item)
            tree_host_->setCurrentItem(item);
        emit sig_contextMenu(tree_host_->viewport()->mapToGlobal(pos));
    });

    // Router searches are issued only after the user stops typing, so a request is not sent on
    // every keystroke. Local results are shown immediately by search().
    router_search_timer_ = new QTimer(this);
    router_search_timer_->setSingleShot(true);
    router_search_timer_->setInterval(300);
    connect(router_search_timer_, &QTimer::timeout, this, &SearchWidget::dispatchRouterSearch);

    layout->addWidget(tree_host_);
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
    tree_host_->clear();

    if (query.isEmpty())
    {
        router_search_timer_->stop();
        updateStatusLabels();
        return;
    }

    Database& db = Database::instance();

    QHash<qint64, GroupConfig> groups;
    const QList<GroupConfig> all_groups = db.allGroups();
    groups.reserve(all_groups.size());
    for (const GroupConfig& group : std::as_const(all_groups))
        groups.insert(group.id(), group);

    const QList<HostConfig> results = db.searchHosts(query);

    for (const HostConfig& host : std::as_const(results))
        new LocalItem(host, buildGroupPath(host.groupId(), groups), tree_host_);

    updateStatusLabels();

    // Local results are shown right away; router workspaces are queried after a short debounce.
    router_search_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::dispatchRouterSearch()
{
    const QString query = current_query_;
    if (query.isEmpty())
        return;

    const QList<RouterConfig> routers = Database::instance().routerList();
    for (const RouterConfig& config : std::as_const(routers))
    {
        Router* router = Router::instance(config.routerId());
        if (!router || router->status() != Router::Status::ONLINE)
            continue;

        const qint64 router_id = config.routerId();
        const QString source_label = config.displayLabel();
        router->searchHosts(query, this, [this, query, router_id, source_label](
            const Router::HostList& list)
        {
            addRouterHosts(query, router_id, source_label, list);
        });
    }
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::addRouterHosts(const QString& query, qint64 router_id,
                                  const QString& source_label, const Router::HostList& list)
{
    // Drop a late response whose query no longer matches what the user is searching for.
    if (query != current_query_)
        return;

    for (const Router::Host& host : std::as_const(list.hosts))
        new RouterItem(router_id, host, source_label, tree_host_);

    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::clear()
{
    router_search_timer_->stop();
    current_query_.clear();
    highlight_delegate_->setQuery(QString());
    tree_host_->clear();
    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
SearchWidget::Item* SearchWidget::currentItem()
{
    return static_cast<Item*>(tree_host_->currentItem());
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::setCurrentHost(qint64 entry_id)
{
    Item* item = findItemByEntryId(entry_id);
    if (!item)
        return;

    tree_host_->setCurrentItem(item);
    tree_host_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::refreshItem(qint64 entry_id)
{
    LocalItem* item = findItemByEntryId(entry_id);
    if (!item)
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

    item->updateFrom(*updated, buildGroupPath(updated->groupId(), groups));
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::removeItem(qint64 entry_id)
{
    Item* item = findItemByEntryId(entry_id);
    if (!item)
        return;

    int row = tree_host_->indexOfTopLevelItem(item);
    delete tree_host_->takeTopLevelItem(row);

    int count = tree_host_->topLevelItemCount();
    if (count > 0)
    {
        int next_row = qMin(row, count - 1);
        tree_host_->setCurrentItem(tree_host_->topLevelItem(next_row));
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
void SearchWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        QStringList headers;
        headers << tr("Name") << tr("Address / ID") << tr("Group") << tr("Comment");
        tree_host_->setHeaderLabels(headers);
    }
    ContentWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = tree_host_->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(tree_host_->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
SearchWidget::LocalItem* SearchWidget::findItemByEntryId(qint64 entry_id) const
{
    const int count = tree_host_->topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        LocalItem* item = dynamic_cast<LocalItem*>(tree_host_->topLevelItem(i));
        if (item && item->entryId() == entry_id)
            return item;
    }
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::updateStatusLabels()
{
    status_results_label_->setText(tr("%n result(s)", "", tree_host_->topLevelItemCount()));
}
