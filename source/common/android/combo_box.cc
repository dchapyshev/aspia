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

#include "common/android/combo_box.h"

#include <QApplication>
#include <QFrame>
#include <QIcon>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QScroller>
#include <QScrollerProperties>
#include <QStyledItemDelegate>
#include <QVariantAnimation>

#include "common/android/controls.h"
#include "common/android/scroll_indicator.h"

namespace {

constexpr int kFieldHeight = 46;
constexpr int kCornerRadius = 6;
constexpr int kHorizontalPadding = 13;
constexpr int kLabelPadding = 4;
constexpr int kArrowWidth = 12;
constexpr int kArrowHeight = 7;
constexpr int kArrowSpacing = 8;
constexpr int kFieldIconSize = 22;
constexpr int kFieldIconSpacing = 10;
constexpr int kItemHeight = 48;
constexpr int kIconSize = 24;
constexpr int kIconSpacing = 12;
constexpr int kMaxVisibleItems = 8;
constexpr int kPopupRadius = 8;
constexpr double kFloatedLabelScale = 0.78;
constexpr double kDisabledOpacity = 0.38;
constexpr double kMutedLabelOpacity = 0.55;
constexpr double kSelectedLayerOpacity = 0.12;
constexpr double kHoverLayerOpacity = 0.08;

//--------------------------------------------------------------------------------------------------
QPainterPath itemPath(const QRectF& rect, double radius, bool round_top, bool round_bottom)
{
    const double top = round_top ? radius : 0.0;
    const double bottom = round_bottom ? radius : 0.0;

    QPainterPath path;
    path.moveTo(rect.left(), rect.top() + top);
    if (round_top)
        path.arcTo(rect.left(), rect.top(), top * 2, top * 2, 180, -90);
    path.lineTo(rect.right() - top, rect.top());
    if (round_top)
        path.arcTo(rect.right() - top * 2, rect.top(), top * 2, top * 2, 90, -90);
    path.lineTo(rect.right(), rect.bottom() - bottom);
    if (round_bottom)
        path.arcTo(rect.right() - bottom * 2, rect.bottom() - bottom * 2, bottom * 2, bottom * 2,
                   0, -90);
    path.lineTo(rect.left() + bottom, rect.bottom());
    if (round_bottom)
        path.arcTo(rect.left(), rect.bottom() - bottom * 2, bottom * 2, bottom * 2, 270, -90);
    path.closeSubpath();
    return path;
}

// Forces the list-style drop-down instead of the menu-style popup. The menu style (reported by the
// Android platform style) ignores the visible item limit and manages its own scrolling, which
// fights the kinetic scroller, so it is turned off.
class PopupStyle final : public QProxyStyle
{
public:
    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* return_data) const final
    {
        if (hint == QStyle::SH_ComboBox_Popup)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, return_data);
    }
};

// List view that paints a rounded surface with an outline for the drop-down. The popup container
// is made translucent, so the area outside the rounded corners shows the content behind it.
class PopupView final : public QListView
{
public:
    explicit PopupView(QWidget* parent)
        : QListView(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // Per-pixel scrolling, otherwise the kinetic scroller quantizes to whole items and jumps.
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        viewport()->setAutoFillBackground(false);

        // Long lists do not fit the screen, so kinetic finger scrolling is enabled. The mouse
        // gesture is used because Qt on Android synthesizes mouse events from touches, and the
        // overshoot bounce is disabled so the list rests cleanly at its ends.
        QScroller::grabGesture(viewport(), QScroller::LeftMouseButtonGesture);

        QScroller* scroller = QScroller::scroller(viewport());
        QScrollerProperties properties = scroller->scrollerProperties();
        properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                                   QScrollerProperties::OvershootAlwaysOff);
        properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                                   QScrollerProperties::OvershootAlwaysOff);
        scroller->setScrollerProperties(properties);

        new ScrollIndicator(this, kPopupRadius);
    }

protected:
    // QListView implementation.
    void paintEvent(QPaintEvent* event) final
    {
        {
            QPainter painter(viewport());
            painter.setRenderHint(QPainter::Antialiasing);

            QRectF surface = viewport()->rect();
            surface.adjust(0.5, 0.5, -0.5, -0.5);

            // The application palette is used directly: the cached popup widgets may still hold the
            // previous theme's palette on the first open after a theme change, which would flash.
            const QPalette palette = QApplication::palette();
            painter.setPen(QPen(palette.color(QPalette::Mid), 1));
            painter.setBrush(palette.color(QPalette::Base));
            painter.drawRoundedRect(surface, kPopupRadius, kPopupRadius);
        }

        QListView::paintEvent(event);
    }
};

// Draws the drop-down items as tall touch targets with a press/selection state layer. The first
// and last items round their outer corners so the selection does not overlap the rounded popup.
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

        // The application palette is used directly so the items match the current theme even on
        // the first open after a theme change, when the cached view palette may be stale.
        const QPalette palette = QApplication::palette();

        double layer_opacity = 0.0;
        if (option.state & QStyle::State_Selected)
            layer_opacity = kSelectedLayerOpacity;
        else if (option.state & QStyle::State_MouseOver)
            layer_opacity = kHoverLayerOpacity;

        if (layer_opacity > 0.0)
        {
            QColor layer = Controls::accentColor();
            layer.setAlphaF(layer_opacity);

            const bool first = index.row() == 0;
            const bool last = index.row() == index.model()->rowCount() - 1;

            // Inset by the outline width on the touching edges so the layer nests inside the
            // rounded outline instead of poking past it at the corners.
            QRectF rect = option.rect;
            rect.adjust(1, first ? 1 : 0, -1, last ? -1 : 0);

            if (first || last)
                painter->fillPath(itemPath(rect, kPopupRadius - 1, first, last), layer);
            else
                painter->fillRect(rect, layer);
        }

        const bool rtl = (option.direction == Qt::RightToLeft);
        QRect content = option.rect.adjusted(kHorizontalPadding, 0, -kHorizontalPadding, 0);

        // An optional leading icon, drawn with its own colors (e.g. a drive icon).
        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull())
        {
            const QRect icon_rect(rtl ? content.right() - kIconSize : content.left(),
                                  content.center().y() - kIconSize / 2, kIconSize, kIconSize);
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
        painter->setPen(palette.color(QPalette::Text));
        painter->drawText(content, Qt::AlignVCenter | Qt::AlignAbsolute | alignment, elided);

        painter->restore();
    }
};

} // namespace

//--------------------------------------------------------------------------------------------------
ComboBox::ComboBox(QWidget* parent)
    : QComboBox(parent),
      focus_animation_(Controls::createAnimation(this)),
      focus_progress_(0.0)
{
    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    // The proxy style is parented to the widget so it is destroyed with it.
    PopupStyle* popup_style = new PopupStyle();
    popup_style->setParent(this);
    setStyle(popup_style);
    setMaxVisibleItems(kMaxVisibleItems);

    setView(new PopupView(this));
    setItemDelegate(new ItemDelegate(this));
    view()->setFont(font());

    if (QFrame* container = qobject_cast<QFrame*>(view()->parentWidget()))
    {
        // The container restores its rectangular frame on style changes, so the frame is removed
        // with a style sheet instead of setFrameShape().
        container->setStyleSheet(
            "QComboBoxPrivateContainer { background: transparent; border: none; }");
        container->setAttribute(Qt::WA_TranslucentBackground);
    }

    connect(focus_animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        focus_progress_ = value.toDouble();
        update();
    });
}

//--------------------------------------------------------------------------------------------------
ComboBox::~ComboBox() = default;

//--------------------------------------------------------------------------------------------------
void ComboBox::setLabel(const QString& label)
{
    if (label_ == label)
        return;

    label_ = label;
    update();
}

//--------------------------------------------------------------------------------------------------
QSize ComboBox::sizeHint() const
{
    return QSize(QComboBox::sizeHint().width(), kFieldHeight + labelOverflow());
}

//--------------------------------------------------------------------------------------------------
QSize ComboBox::minimumSizeHint() const
{
    return QSize(QComboBox::minimumSizeHint().width(), kFieldHeight + labelOverflow());
}

//--------------------------------------------------------------------------------------------------
void ComboBox::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!isEnabled())
        painter.setOpacity(kDisabledOpacity);

    const bool rtl = (layoutDirection() == Qt::RightToLeft);

    const QColor accent = Controls::accentColor();
    const QColor outline = palette().color(QPalette::Mid);

    QRectF field = rect();
    field.setTop(labelOverflow());

    // The label always rests on the outline, because the field always shows the current value.
    QFont label_font = Controls::scaledFont(font(), kFloatedLabelScale);
    QRectF label_rect;
    if (!label_.isEmpty())
    {
        const QFontMetricsF label_metrics(label_font);
        const double label_width = label_metrics.horizontalAdvance(label_);
        const double label_center_x = rtl ?
            field.right() - kHorizontalPadding - kLabelPadding - label_width / 2.0 :
            field.left() + kHorizontalPadding + kLabelPadding + label_width / 2.0;

        label_rect = QRectF(0, 0, label_width + kLabelPadding * 2, label_metrics.height());
        label_rect.moveCenter(QPointF(label_center_x, field.top()));
    }

    // The outline thickens and is repainted with the accent color while the field has focus. The
    // label interrupts the outline stroke, so its area is clipped out.
    painter.save();
    if (!label_rect.isNull())
        painter.setClipRegion(QRegion(rect()) - QRegion(label_rect.toAlignedRect()));

    const double border_width = Controls::lerp(1.0, 2.0, focus_progress_);
    painter.setPen(QPen(Controls::blend(outline, accent, focus_progress_), border_width));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(field.adjusted(border_width / 2.0, border_width / 2.0,
                                           -border_width / 2.0, -border_width / 2.0),
                            kCornerRadius, kCornerRadius);
    painter.restore();

    if (!label_rect.isNull())
    {
        QColor label_color = Controls::blend(palette().color(QPalette::WindowText), accent,
                                             focus_progress_);
        label_color.setAlphaF(Controls::lerp(kMutedLabelOpacity, 1.0, focus_progress_));

        painter.setFont(label_font);
        painter.setPen(label_color);
        painter.drawText(label_rect, Qt::AlignCenter, label_);
    }

    // The drop-down indicator is a triangle pointing down on the trailing edge.
    const double arrow_center_x = rtl ?
        field.left() + kHorizontalPadding + kArrowWidth / 2.0 :
        field.right() - kHorizontalPadding - kArrowWidth / 2.0;
    const double arrow_top = field.center().y() - kArrowHeight / 2.0;

    QPolygonF arrow;
    arrow << QPointF(arrow_center_x - kArrowWidth / 2.0, arrow_top)
          << QPointF(arrow_center_x + kArrowWidth / 2.0, arrow_top)
          << QPointF(arrow_center_x, arrow_top + kArrowHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::WindowText));
    painter.drawPolygon(arrow);

    if (!currentText().isEmpty())
    {
        QRectF text_rect = field;
        if (rtl)
        {
            text_rect.setLeft(arrow_center_x + kArrowWidth / 2.0 + kArrowSpacing);
            text_rect.setRight(field.right() - kHorizontalPadding);
        }
        else
        {
            text_rect.setLeft(field.left() + kHorizontalPadding);
            text_rect.setRight(arrow_center_x - kArrowWidth / 2.0 - kArrowSpacing);
        }

        // An optional leading icon for the current item, drawn with its own colors.
        const QIcon icon = (currentIndex() >= 0) ? itemIcon(currentIndex()) : QIcon();
        if (!icon.isNull())
        {
            const QRect icon_rect(
                rtl ? int(text_rect.right()) - kFieldIconSize : int(text_rect.left()),
                int(text_rect.center().y()) - kFieldIconSize / 2, kFieldIconSize, kFieldIconSize);
            icon.paint(&painter, icon_rect);

            if (rtl)
                text_rect.setRight(icon_rect.left() - kFieldIconSpacing);
            else
                text_rect.setLeft(icon_rect.right() + kFieldIconSpacing);
        }

        const QFontMetricsF metrics(font());
        const QString elided = metrics.elidedText(currentText(), Qt::ElideRight, text_rect.width());

        painter.setFont(font());
        painter.setPen(palette().color(QPalette::WindowText));
        // AlignAbsolute keeps the visual side fixed: the painter mirrors plain alignment flags
        // when the widget layout direction is right-to-left.
        painter.drawText(text_rect,
                         Qt::AlignVCenter | Qt::AlignAbsolute | (rtl ? Qt::AlignRight : Qt::AlignLeft),
                         elided);
    }
}

//--------------------------------------------------------------------------------------------------
void ComboBox::focusInEvent(QFocusEvent* event)
{
    QComboBox::focusInEvent(event);

    focus_animation_->stop();
    focus_animation_->setStartValue(focus_progress_);
    focus_animation_->setEndValue(1.0);
    focus_animation_->start();
}

//--------------------------------------------------------------------------------------------------
void ComboBox::focusOutEvent(QFocusEvent* event)
{
    QComboBox::focusOutEvent(event);

    focus_animation_->stop();
    focus_animation_->setStartValue(focus_progress_);
    focus_animation_->setEndValue(0.0);
    focus_animation_->start();
}

//--------------------------------------------------------------------------------------------------
int ComboBox::labelOverflow() const
{
    // Without a label there is nothing resting on the outline, so no extra room is reserved above it.
    if (label_.isEmpty())
        return 0;

    return QFontMetrics(Controls::scaledFont(font(), kFloatedLabelScale)).height() / 2;
}
