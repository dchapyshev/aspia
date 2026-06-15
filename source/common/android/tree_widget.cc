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

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScroller>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>

#include "common/android/controls.h"
#include "common/android/scroll_indicator.h"

namespace {

constexpr int kItemHeight = 48;
constexpr int kHorizontalPadding = 13;
constexpr int kIconSize = 24;
constexpr int kIconSpacing = 12;
constexpr int kChevronWidth = 12;
constexpr int kChevronHeight = 7;
constexpr int kChevronAreaWidth = 44;
constexpr int kIndentation = 16;
constexpr double kSelectedLayerOpacity = 0.12;
constexpr double kHoverLayerOpacity = 0.08;
constexpr double kChevronOpacity = 0.6;
constexpr int kLongPressTimeout = 500;
constexpr int kLongPressMoveThreshold = 16;

//--------------------------------------------------------------------------------------------------
QRect chevronArea(const QRect& content, bool rtl)
{
    return rtl ? QRect(content.left(), content.top(), kChevronAreaWidth, content.height()) :
                 QRect(content.right() - kChevronAreaWidth + 1, content.top(), kChevronAreaWidth,
                       content.height());
}

// Draws tree items as tall touch targets with a leading icon and, for rows that have children,
// a trailing accordion chevron (down when collapsed, up when expanded). The row background and the
// state layer are drawn by the view, so the delegate paints the content only.
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

        if (index.model()->hasChildren(index))
        {
            const QRect area = chevronArea(content, rtl);
            drawChevron(painter, area, option.state & QStyle::State_Open,
                        option.palette.color(QPalette::WindowText));

            if (rtl)
                content.setLeft(area.right() + 1);
            else
                content.setRight(area.left() - 1);
        }

        const QString elided = option.fontMetrics.elidedText(
            index.data(Qt::DisplayRole).toString(), Qt::ElideRight, content.width());
        const Qt::Alignment alignment = rtl ? Qt::AlignRight : Qt::AlignLeft;

        painter->setFont(option.font);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(content, Qt::AlignVCenter | Qt::AlignAbsolute | alignment, elided);

        painter->restore();
    }

private:
    static void drawChevron(QPainter* painter, const QRect& area, bool expanded, QColor color)
    {
        const QPointF center = area.center();
        color.setAlphaF(kChevronOpacity);

        QPolygonF chevron;
        if (expanded)
        {
            chevron << QPointF(center.x() - kChevronWidth / 2.0, center.y() + kChevronHeight / 2.0)
                    << QPointF(center.x() + kChevronWidth / 2.0, center.y() + kChevronHeight / 2.0)
                    << QPointF(center.x(), center.y() - kChevronHeight / 2.0);
        }
        else
        {
            chevron << QPointF(center.x() - kChevronWidth / 2.0, center.y() - kChevronHeight / 2.0)
                    << QPointF(center.x() + kChevronWidth / 2.0, center.y() - kChevronHeight / 2.0)
                    << QPointF(center.x(), center.y() + kChevronHeight / 2.0);
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawPolygon(chevron);
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

    // Long press is detected from the synthesized mouse stream: a press starts a timer that, unless
    // a move or a scroll cancels it first, reports a long press on the pressed row.
    long_press_timer_ = new QTimer(this);
    long_press_timer_->setSingleShot(true);
    long_press_timer_->setInterval(kLongPressTimeout);
    connect(long_press_timer_, &QTimer::timeout, this, [this]()
    {
        if (QTreeWidgetItem* item = itemAt(press_pos_))
            emit sig_itemLongPressed(item);
    });

    connect(QScroller::scroller(viewport()), &QScroller::stateChanged, this,
            [this](QScroller::State state)
    {
        if (state == QScroller::Dragging || state == QScroller::Scrolling)
            long_press_timer_->stop();
    });

    applyBackgroundColor();

    new ScrollIndicator(this);
}

//--------------------------------------------------------------------------------------------------
TreeWidget::~TreeWidget() = default;

//--------------------------------------------------------------------------------------------------
void TreeWidget::paintEvent(QPaintEvent* event)
{
    QTreeWidget::paintEvent(event);

    // A top separator that sets the list off from whatever is above it, mirroring the navigation
    // bar border.
    QPainter painter(viewport());
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(0, 0, viewport()->width(), 0);
}

//--------------------------------------------------------------------------------------------------
void TreeWidget::changeEvent(QEvent* event)
{
    QTreeWidget::changeEvent(event);

    // The base color is reset from the theme on a palette change; the guard skips the change that
    // applyBackgroundColor() raises itself.
    if ((event->type() == QEvent::ApplicationPaletteChange ||
         event->type() == QEvent::PaletteChange) && !applying_palette_)
    {
        applyBackgroundColor();
    }
}

//--------------------------------------------------------------------------------------------------
void TreeWidget::mousePressEvent(QMouseEvent* event)
{
    press_pos_ = event->position().toPoint();
    long_press_timer_->start();

    QTreeWidget::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TreeWidget::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->position().toPoint() - press_pos_).manhattanLength() > kLongPressMoveThreshold)
        long_press_timer_->stop();

    QTreeWidget::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TreeWidget::mouseReleaseEvent(QMouseEvent* event)
{
    long_press_timer_->stop();

    const QPoint pos = event->position().toPoint();
    const QModelIndex index = indexAt(pos);

    // A tap on the trailing chevron of a parent row toggles it instead of activating the row.
    if (index.isValid() && model()->hasChildren(index))
    {
        const bool rtl = (layoutDirection() == Qt::RightToLeft);
        const QRect content =
            visualRect(index).adjusted(kHorizontalPadding, 0, -kHorizontalPadding, 0);

        if (chevronArea(content, rtl).contains(pos))
        {
            setExpanded(index, !isExpanded(index));
            event->accept();
            return;
        }
    }

    QTreeWidget::mouseReleaseEvent(event);
}

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
void TreeWidget::drawBranches(QPainter* /* painter */, const QRect& /* rect */,
                              const QModelIndex& /* index */) const
{
    // The expand indicator is a trailing accordion chevron drawn by the delegate, so the leading
    // branch area is left empty.
}

//--------------------------------------------------------------------------------------------------
void TreeWidget::applyBackgroundColor()
{
    QPalette pal = palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Window));

    applying_palette_ = true;
    setPalette(pal);
    applying_palette_ = false;
}
