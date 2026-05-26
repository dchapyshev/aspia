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
#include <QTreeWidget>
#include <QVBoxLayout>

#include <optional>

#include "base/logging.h"
#include "client/database.h"

namespace {

const int kColumnName    = 0;
const int kColumnAddress = 1;
const int kColumnGroup   = 2;
const int kColumnComment = 3;

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
SearchWidget::Item::Item(const HostConfig& host, const QString& group_path, QTreeWidget* parent)
    : QTreeWidgetItem(parent)
{
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
    updateFrom(host, group_path);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::Item::updateFrom(const HostConfig& host, const QString& group_path)
{
    computer_ = host;

    QString single_line_comment = host.comment();
    single_line_comment.replace('\n', ' ').replace('\r', ' ');

    setText(kColumnName, host.name());
    setText(kColumnAddress, host.address());
    setText(kColumnGroup, group_path);
    setToolTip(kColumnGroup, group_path);
    setText(kColumnComment, single_line_comment);
    setToolTip(kColumnComment, host.comment());
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

    tree_computer_ = new QTreeWidget(this);
    tree_computer_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_computer_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_computer_->setIndentation(0);
    tree_computer_->setSortingEnabled(true);
    tree_computer_->setColumnCount(4);

    QStringList headers;
    headers << tr("Name") << tr("Address / ID") << tr("Group") << tr("Comment");
    tree_computer_->setHeaderLabels(headers);

    QHeaderView* header = tree_computer_->header();
    header->resizeSection(kColumnName, 200);
    header->resizeSection(kColumnAddress, 180);
    header->resizeSection(kColumnGroup, 150);
    header->resizeSection(kColumnComment, 250);

    highlight_delegate_ = new HighlightDelegate(this);
    tree_computer_->setItemDelegateForColumn(kColumnName, highlight_delegate_);
    tree_computer_->setItemDelegateForColumn(kColumnAddress, highlight_delegate_);
    tree_computer_->setItemDelegateForColumn(kColumnGroup, highlight_delegate_);
    tree_computer_->setItemDelegateForColumn(kColumnComment, highlight_delegate_);

    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::customContextMenuRequested, this, &SearchWidget::onHeaderContextMenu);

    connect(tree_computer_, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        if (!item)
            return;

        Item* computer_item = static_cast<Item*>(item);
        emit sig_doubleClicked(computer_item->computerId());
    });

    connect(tree_computer_, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
    {
        qint64 entry_id = -1;

        if (current)
        {
            Item* computer_item = static_cast<Item*>(current);
            entry_id = computer_item->computerId();
        }

        emit sig_currentChanged(entry_id);
    });

    connect(tree_computer_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        qint64 entry_id = 0;
        Item* item = static_cast<Item*>(tree_computer_->itemAt(pos));
        if (item)
        {
            tree_computer_->setCurrentItem(item);
            entry_id = item->computerId();
        }
        emit sig_contextMenu(entry_id, tree_computer_->viewport()->mapToGlobal(pos));
    });

    layout->addWidget(tree_computer_);
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
    tree_computer_->clear();

    if (query.isEmpty())
    {
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
        new Item(host, buildGroupPath(host.groupId(), groups), tree_computer_);

    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::clear()
{
    current_query_.clear();
    highlight_delegate_->setQuery(QString());
    tree_computer_->clear();
    updateStatusLabels();
}

//--------------------------------------------------------------------------------------------------
SearchWidget::Item* SearchWidget::currentItem()
{
    return static_cast<Item*>(tree_computer_->currentItem());
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::setCurrentHost(qint64 entry_id)
{
    Item* item = findItemByComputerId(entry_id);
    if (!item)
        return;

    tree_computer_->setCurrentItem(item);
    tree_computer_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::refreshItem(qint64 entry_id)
{
    Item* item = findItemByComputerId(entry_id);
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
    Item* item = findItemByComputerId(entry_id);
    if (!item)
        return;

    int row = tree_computer_->indexOfTopLevelItem(item);
    delete tree_computer_->takeTopLevelItem(row);

    int count = tree_computer_->topLevelItemCount();
    if (count > 0)
    {
        int next_row = qMin(row, count - 1);
        tree_computer_->setCurrentItem(tree_computer_->topLevelItem(next_row));
        tree_computer_->setFocus();
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

        stream << tree_computer_->header()->saveState();
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
        tree_computer_->header()->restoreState(columns_state);
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
        tree_computer_->setHeaderLabels(headers);
    }
    ContentWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = tree_computer_->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(tree_computer_->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
SearchWidget::Item* SearchWidget::findItemByComputerId(qint64 entry_id) const
{
    const int count = tree_computer_->topLevelItemCount();
    for (int i = 0; i < count; ++i)
    {
        Item* item = static_cast<Item*>(tree_computer_->topLevelItem(i));
        if (item->computerId() == entry_id)
            return item;
    }
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::updateStatusLabels()
{
    status_results_label_->setText(tr("%n result(s)", "", tree_computer_->topLevelItemCount()));
}
