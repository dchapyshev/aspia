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

#include "common/android/tree_widget.h"

#include <QPainter>
#include <QScroller>
#include <QStyledItemDelegate>

#include "common/android/controls.h"
#include "common/android/scroll_indicator.h"

namespace {

constexpr int kItemHeight = 48;
constexpr int kHorizontalPadding = 13;
constexpr int kIconSize = 24;
constexpr int kIconSpacing = 12;
constexpr int kChevronWidth = 12;
constexpr int kChevronHeight = 7;
constexpr int kIndentation = 28;
constexpr double kSelectedLayerOpacity = 0.12;
constexpr double kHoverLayerOpacity = 0.08;
constexpr double kChevronOpacity = 0.6;

// Draws tree items as tall touch targets with an optional leading icon. The row background and
// the state layer are drawn by the view, so the delegate paints the content only.
class ItemDelegate final : public QStyledItemDelegate
{
public:
    explicit ItemDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
        // Nothing.
    }

    // QStyledItemDelegate implementation.
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const final
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), kItemHeight));
        return size;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const final
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const bool rtl = (option.direction == Qt::RightToLeft);

        QRect content = rtl ? option.rect.adjusted(kHorizontalPadding, 0, 0, 0) :
                              option.rect.adjusted(0, 0, -kHorizontalPadding, 0);

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

        const QString elided = option.fontMetrics.elidedText(
            index.data(Qt::DisplayRole).toString(), Qt::ElideRight, content.width());
        const Qt::Alignment alignment = rtl ? Qt::AlignRight : Qt::AlignLeft;

        painter->setFont(option.font);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(content, Qt::AlignVCenter | Qt::AlignAbsolute | alignment, elided);

        painter->restore();
    }
};

} // namespace

//--------------------------------------------------------------------------------------------------
TreeWidget::TreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    setHeaderHidden(true);
    setFrameShape(QFrame::NoFrame);
    setIndentation(kIndentation);
    setUniformRowHeights(true);
    setAnimated(true);
    setExpandsOnDoubleClick(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setItemDelegate(new ItemDelegate(this));

    // Qt on Android synthesizes mouse events from touches, so the scroller listens to the mouse
    // gesture: grabbing the touch gesture would swallow row taps.
    QScroller::grabGesture(viewport(), QScroller::LeftMouseButtonGesture);

    new ScrollIndicator(this);

    connect(this, &QTreeWidget::itemClicked, this, &TreeWidget::onItemClicked);
}

//--------------------------------------------------------------------------------------------------
TreeWidget::~TreeWidget() = default;

//--------------------------------------------------------------------------------------------------
void TreeWidget::drawRow(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const
{
    // The state layer spans the full row width, including the branch area.
    double layer_opacity = 0.0;
    if (selectionModel()->isSelected(index))
        layer_opacity = kSelectedLayerOpacity;
    else if (option.state & QStyle::State_MouseOver)
        layer_opacity = kHoverLayerOpacity;

    if (layer_opacity > 0.0)
    {
        QColor layer = palette().color(QPalette::Highlight);
        layer.setAlphaF(layer_opacity);
        painter->fillRect(QRect(0, option.rect.top(), viewport()->width(), option.rect.height()),
                          layer);
    }

    // The base implementation paints the default selection panel with the highlight color, which
    // is made transparent because the state layer is already drawn.
    QStyleOptionViewItem row_option(option);
    row_option.palette.setColor(QPalette::Highlight, Qt::transparent);
    QTreeWidget::drawRow(painter, row_option, index);
}

//--------------------------------------------------------------------------------------------------
void TreeWidget::drawBranches(QPainter* painter, const QRect& rect,
                              const QModelIndex& index) const
{
    if (!model()->hasChildren(index))
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // The chevron is centered in the branch section adjacent to the content: it points towards
    // the content when collapsed and down when expanded.
    const bool rtl = (layoutDirection() == Qt::RightToLeft);
    const QRectF section = rtl ?
        QRectF(rect.left(), rect.top(), indentation(), rect.height()) :
        QRectF(rect.right() - indentation(), rect.top(), indentation(), rect.height());
    const QPointF center = section.center();

    QColor color = palette().color(QPalette::WindowText);
    color.setAlphaF(kChevronOpacity);

    QPolygonF chevron;
    if (isExpanded(index))
    {
        chevron << QPointF(center.x() - kChevronWidth / 2.0, center.y() - kChevronHeight / 2.0)
                << QPointF(center.x() + kChevronWidth / 2.0, center.y() - kChevronHeight / 2.0)
                << QPointF(center.x(), center.y() + kChevronHeight / 2.0);
    }
    else if (rtl)
    {
        chevron << QPointF(center.x() + kChevronHeight / 2.0, center.y() - kChevronWidth / 2.0)
                << QPointF(center.x() + kChevronHeight / 2.0, center.y() + kChevronWidth / 2.0)
                << QPointF(center.x() - kChevronHeight / 2.0, center.y());
    }
    else
    {
        chevron << QPointF(center.x() - kChevronHeight / 2.0, center.y() - kChevronWidth / 2.0)
                << QPointF(center.x() - kChevronHeight / 2.0, center.y() + kChevronWidth / 2.0)
                << QPointF(center.x() + kChevronHeight / 2.0, center.y());
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPolygon(chevron);

    painter->restore();
}

//--------------------------------------------------------------------------------------------------
void TreeWidget::onItemClicked(QTreeWidgetItem* item)
{
    // Parent rows expand and collapse with a tap anywhere on the row.
    if (item->childCount() > 0)
        item->setExpanded(!item->isExpanded());
}
