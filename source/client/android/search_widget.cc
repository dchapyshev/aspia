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

#include "client/android/search_widget.h"

#include <QAbstractTextDocumentLayout>
#include <QHeaderView>
#include <QIcon>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTextOption>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "common/android/label.h"
#include "common/android/tree_widget.h"

namespace {

constexpr int kDataRole = Qt::UserRole;

// Match the touch metrics of the default TreeWidget delegate so highlighted rows look the same.
constexpr int kItemHeight = 48;
constexpr int kHorizontalPadding = 13;
constexpr int kIconSize = 24;
constexpr int kIconSpacing = 12;

//--------------------------------------------------------------------------------------------------
QString buildHighlightedHtml(const QString& text, const QString& query)
{
    QString result;

    int from = 0;
    const int query_len = query.length();

    while (from < text.length())
    {
        const int idx = text.indexOf(query, from, Qt::CaseInsensitive);
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

} // namespace

// Touch row delegate that highlights the parts of the text matching the current query. Replicates
// the icon-over-text layout of the default TreeWidget delegate so the rows match the rest of the app.
class SearchHighlightDelegate final : public QStyledItemDelegate
{
public:
    explicit SearchHighlightDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
        // Nothing.
    }

    void setQuery(const QString& query) { query_ = query; }

    // QStyledItemDelegate implementation.
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const final
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), kItemHeight));
        size.setWidth(size.width() + 2 * kHorizontalPadding);
        return size;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const final
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const bool rtl = (option.direction == Qt::RightToLeft);
        QRect content = option.rect.adjusted(kHorizontalPadding, 0, -kHorizontalPadding, 0);

        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull())
        {
            const int icon_y = content.center().y() - kIconSize / 2;
            const QRect icon_rect = rtl ?
                QRect(content.right() - kIconSize, icon_y, kIconSize, kIconSize) :
                QRect(content.left(), icon_y, kIconSize, kIconSize);

            icon.paint(painter, icon_rect);

            if (rtl)
                content.setRight(icon_rect.left() - kIconSpacing);
            else
                content.setLeft(icon_rect.right() + kIconSpacing);
        }

        const QString text = index.data(Qt::DisplayRole).toString();
        const QString elided =
            option.fontMetrics.elidedText(text, Qt::ElideRight, content.width());
        const QColor foreground = option.palette.color(QPalette::Text);

        if (query_.isEmpty() || !text.contains(query_, Qt::CaseInsensitive))
        {
            const Qt::Alignment alignment = rtl ? Qt::AlignRight : Qt::AlignLeft;
            painter->setFont(option.font);
            painter->setPen(foreground);
            painter->drawText(content, Qt::AlignVCenter | Qt::AlignAbsolute | alignment, elided);
        }
        else
        {
            QTextOption text_option;
            text_option.setWrapMode(QTextOption::NoWrap);

            QTextDocument doc;
            doc.setDefaultFont(option.font);
            doc.setDocumentMargin(0);
            doc.setDefaultTextOption(text_option);
            doc.setDefaultStyleSheet(QString("body { color: %1; }").arg(foreground.name()));
            doc.setHtml(buildHighlightedHtml(elided, query_));
            doc.setTextWidth(content.width());

            const qreal y_offset = (content.height() - doc.size().height()) / 2.0;

            painter->setClipRect(content);
            painter->translate(content.left(), content.top() + qMax(qreal(0), y_offset));
            doc.documentLayout()->draw(painter, QAbstractTextDocumentLayout::PaintContext());
        }

        painter->restore();
    }

private:
    QString query_;
};

//--------------------------------------------------------------------------------------------------
SearchWidget::SearchWidget(QWidget* parent)
    : QWidget(parent),
      results_(new TreeWidget()),
      delegate_(new SearchHighlightDelegate(this)),
      empty_label_(new Label(QString(), Label::Role::CAPTION))
{
    results_->setColumnCount(2);
    results_->setRootIsDecorated(false);
    results_->setItemDelegate(delegate_);
    results_->header()->setStretchLastSection(false);
    results_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    results_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    empty_label_->setText(tr("Nothing found"));
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setVisible(false);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(empty_label_);
    layout->addWidget(results_, 1);

    connect(results_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int)
    {
        if (item)
            emit sig_activated(item->data(0, kDataRole));
    });
}

//--------------------------------------------------------------------------------------------------
SearchWidget::~SearchWidget() = default;

//--------------------------------------------------------------------------------------------------
void SearchWidget::setResults(const QList<Result>& results, const QString& query)
{
    delegate_->setQuery(query);
    results_->clear();

    for (const Result& result : results)
    {
        QTreeWidgetItem* item =
            new QTreeWidgetItem(results_, { result.title, result.subtitle });
        if (!result.icon_file_path.isEmpty())
            item->setIcon(0, GuiApplication::svgIcon(result.icon_file_path));
        item->setData(0, kDataRole, result.data);
    }

    // The hint distinguishes "no matches" from the initial empty field.
    empty_label_->setVisible(results.isEmpty() && !query.isEmpty());
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::reset()
{
    delegate_->setQuery(QString());
    results_->clear();
    empty_label_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void SearchWidget::retranslate()
{
    empty_label_->setText(tr("Nothing found"));
}
