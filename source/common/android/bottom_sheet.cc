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

#include "common/android/bottom_sheet.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QVariantAnimation>

#include "common/android/controls.h"

namespace {

// Vertical list layout (portrait).
constexpr int kRowHeight = 56;
constexpr int kRowIconSize = 24;
constexpr int kRowIconSpacing = 16;

// Compact button layout (landscape).
constexpr int kCellWidth = 102;
constexpr int kCellHeight = 72;
constexpr int kCellIconSize = 28;
constexpr int kCellRadius = 12;
constexpr int kCaptionTopMargin = 4;
constexpr int kCaptionHPadding = 6;
constexpr double kCaptionFontScale = 0.85;
constexpr int kCaptionMaxLines = 2;

constexpr int kTitleHeight = 44;
constexpr int kTitleSpacing = 16;
constexpr int kHandleWidth = 32;
constexpr int kHandleHeight = 4;
constexpr int kHandleTopMargin = 8;
constexpr int kHorizontalPadding = 16;
constexpr int kVerticalPadding = 8;
constexpr int kCornerRadius = 16;
constexpr double kScrimOpacity = 0.4;
constexpr double kPressLayerOpacity = 0.12;
constexpr int kDragThreshold = 12;

// Hidden gesture: this many taps on the handle strip, no slower than this apart, reveal the statistics.
constexpr int kSecretTapCount = 5;
constexpr int kSecretTapResetMs = 2000;

} // namespace

//--------------------------------------------------------------------------------------------------
BottomSheet::BottomSheet(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setFont(Controls::scaledFont(font(), Controls::kFontScale));

    offset_animation_ = Controls::createAnimation(this);
    connect(offset_animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        offset_ = value.toInt();
        update();
    });
    connect(offset_animation_, &QVariantAnimation::finished, this, [this]()
    {
        if (close_on_finish_)
            close();
    });
}

//--------------------------------------------------------------------------------------------------
BottomSheet::~BottomSheet() = default;

//--------------------------------------------------------------------------------------------------
void BottomSheet::setTitle(const QString& title)
{
    title_ = title;
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::addItem(const QString& text, const QString& icon_file_path, bool selected)
{
    items_.append({ icon_file_path, text, selected });
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::setSelected(int index)
{
    for (int i = 0; i < items_.size(); ++i)
        items_[i].selected = (i == index);

    update();
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::showSheet()
{
    // Cover the whole top-level window so a tap anywhere outside the sheet is caught and closes it.
    QWidget* host = parentWidget() ? parentWidget()->window() : nullptr;
    if (!host)
        return;

    setParent(host);
    setGeometry(host->rect());
    host->installEventFilter(this);

    // Start fully below the bottom edge so the first frame is off-screen, then slide in.
    offset_ = sheetRect().height();

    raise();
    show();

    animateTo(0, false);
}

//--------------------------------------------------------------------------------------------------
bool BottomSheet::eventFilter(QObject* watched, QEvent* event)
{
    // Follow the host window through resizes so the sheet keeps covering it and relays out for the
    // new orientation. The pressed item is dropped because its hit rectangle is now stale.
    if (watched == parentWidget() && event->type() == QEvent::Resize)
    {
        offset_animation_->stop();
        offset_ = 0;
        setGeometry(parentWidget()->rect());
        active_ = -1;
        update();
    }

    return QWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect sheet = sheetRect();

    // The scrim fades out as the sheet slides down so the dismissal reads as one motion.
    const double visible = sheet.height() > 0
        ? qBound(0.0, 1.0 - double(offset_) / sheet.height(), 1.0) : 1.0;
    QColor scrim(0, 0, 0);
    scrim.setAlphaF(kScrimOpacity * visible);
    painter.fillRect(rect(), scrim);

    // The sheet itself rides the drag/animation offset; the scrim above stays put.
    painter.translate(0, offset_);

    // Extend the rounded rect below the bottom edge so only the top corners are rounded.
    painter.setPen(Qt::NoPen);
    painter.setBrush(Controls::surfaceColor());
    painter.drawRoundedRect(QRect(sheet.left(), sheet.top(), sheet.width(), sheet.height() + kCornerRadius),
                            kCornerRadius, kCornerRadius);

    // The drag handle.
    QColor handle_color = palette().color(QPalette::WindowText);
    handle_color.setAlphaF(0.3);
    painter.setBrush(handle_color);
    painter.drawRoundedRect(QRect((width() - kHandleWidth) / 2, sheet.top() + kHandleTopMargin,
                                  kHandleWidth, kHandleHeight),
                            kHandleHeight / 2.0, kHandleHeight / 2.0);

    const bool rtl = (layoutDirection() == Qt::RightToLeft);
    const bool portrait = isPortrait();

    if (!title_.isEmpty())
    {
        QColor title_color = palette().color(QPalette::WindowText);
        title_color.setAlphaF(0.6);
        painter.setPen(title_color);

        QRect title_rect;
        if (portrait)
        {
            // A dedicated row above the list.
            const int title_top = sheet.top() + kHandleTopMargin + kHandleHeight + kVerticalPadding;
            title_rect = QRect(kHorizontalPadding, title_top, width() - 2 * kHorizontalPadding,
                               kTitleHeight);
        }
        else
        {
            // Inline with the buttons row so it does not add an almost empty row above them.
            const int title_width = landscapeTitleWidth() - kTitleSpacing;
            const int title_left = rtl ? width() - kHorizontalPadding - title_width
                                       : kHorizontalPadding;
            title_rect = QRect(title_left, itemsTop(), title_width, landscapeCellHeight());
        }

        painter.drawText(title_rect,
                         Qt::AlignVCenter | Qt::AlignAbsolute | (rtl ? Qt::AlignRight : Qt::AlignLeft),
                         title_);
    }

    const QFont caption_font = Controls::scaledFont(font(), kCaptionFontScale);

    for (int i = 0; i < items_.size(); ++i)
    {
        const QRect item = itemRect(i);

        const QColor foreground = items_[i].selected ?
            Controls::accentColor() : palette().color(QPalette::WindowText);

        if (i == active_)
        {
            QColor layer = Controls::accentColor();
            layer.setAlphaF(kPressLayerOpacity);
            painter.setPen(Qt::NoPen);
            painter.setBrush(layer);
            if (portrait)
                painter.fillRect(item, layer);
            else
                painter.drawRoundedRect(item, kCellRadius, kCellRadius);
        }

        if (portrait)
        {
            if (!items_[i].icon_file_path.isEmpty())
            {
                const QPixmap icon = Controls::tintedPixmap(items_[i].icon_file_path,
                    QSize(kRowIconSize, kRowIconSize), foreground);
                const int icon_x = rtl ? item.right() - kHorizontalPadding - kRowIconSize + 1
                                       : item.left() + kHorizontalPadding;
                painter.drawPixmap(QPoint(icon_x, item.center().y() - kRowIconSize / 2), icon);
            }

            const int icon_margin = kHorizontalPadding + kRowIconSize + kRowIconSpacing;
            const QRect text_rect = rtl ? item.adjusted(kHorizontalPadding, 0, -icon_margin, 0)
                                        : item.adjusted(icon_margin, 0, -kHorizontalPadding, 0);

            // AlignAbsolute keeps the requested side fixed; QPainter would otherwise mirror
            // AlignLeft/AlignRight under an RTL layout.
            painter.setPen(foreground);
            painter.drawText(text_rect,
                             Qt::AlignVCenter | Qt::AlignAbsolute | (rtl ? Qt::AlignRight : Qt::AlignLeft),
                             items_[i].text);
        }
        else
        {
            painter.setFont(caption_font);

            // Center the icon-over-caption block vertically so it lines up with the inline title. The
            // caption may wrap to up to two lines, so reserve that height for every cell in the row.
            const int caption_height = kCaptionMaxLines * painter.fontMetrics().height();
            const int block_height = kCellIconSize + kCaptionTopMargin + caption_height;
            const int block_top = item.top() + (item.height() - block_height) / 2;

            if (!items_[i].icon_file_path.isEmpty())
            {
                const QPixmap icon = Controls::tintedPixmap(items_[i].icon_file_path,
                    QSize(kCellIconSize, kCellIconSize), foreground);
                painter.drawPixmap(QPoint(item.center().x() - kCellIconSize / 2, block_top), icon);
            }

            // Inset horizontally so wrapped captions keep a gap from the neighbouring cells.
            const QRect caption_rect(item.left() + kCaptionHPadding,
                                     block_top + kCellIconSize + kCaptionTopMargin,
                                     item.width() - 2 * kCaptionHPadding, caption_height);
            painter.setPen(foreground);

            // Keep word boundaries when possible, but still break a long single word so a caption
            // without spaces (common after translation) fits the narrow cell instead of overflowing.
            painter.drawText(caption_rect,
                             Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap | Qt::TextWrapAnywhere,
                             items_[i].text);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();

    if (pressed_)
    {
        const int delta = pos.y() - press_y_;
        if (!dragging_ && delta > kDragThreshold)
            dragging_ = true;

        if (dragging_)
        {
            // A downward drag pulls the sheet with the finger; an item cannot be selected mid-drag.
            active_ = -1;
            offset_ = qMax(0, delta);
            update();
        }
        return;
    }

    const int item = itemAt(pos);
    if (item != active_)
    {
        active_ = item;
        update();
    }
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if (!sheetRect().contains(pos))
    {
        dismiss();
        return;
    }

    pressed_ = true;
    dragging_ = false;
    press_y_ = pos.y();
    active_ = itemAt(pos);
    update();
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragging_)
    {
        pressed_ = false;
        dragging_ = false;

        // Dismiss if pulled past a third of its height, otherwise settle back into place.
        if (offset_ > sheetRect().height() / 3)
            dismiss();
        else
            animateTo(0, false);
        return;
    }

    pressed_ = false;

    const QPoint pos = event->position().toPoint();
    const int item = itemAt(pos);
    if (item >= 0)
    {
        emit sig_triggered(item);
        close();
        return;
    }

    // A tap on the handle strip (no item there) drives the hidden statistics gesture.
    if (handleHitRect().contains(pos))
    {
        if (handle_tap_timer_.isValid() && handle_tap_timer_.elapsed() > kSecretTapResetMs)
            handle_taps_ = 0;

        ++handle_taps_;
        handle_tap_timer_.restart();

        if (handle_taps_ >= kSecretTapCount)
        {
            handle_taps_ = 0;
            emit sig_secretGesture();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::animateTo(int target_offset, bool close_after)
{
    close_on_finish_ = close_after;
    offset_animation_->stop();
    offset_animation_->setStartValue(offset_);
    offset_animation_->setEndValue(target_offset);
    offset_animation_->start();
}

//--------------------------------------------------------------------------------------------------
void BottomSheet::dismiss()
{
    animateTo(sheetRect().height(), true);
}

//--------------------------------------------------------------------------------------------------
bool BottomSheet::isPortrait() const
{
    return height() > width();
}

//--------------------------------------------------------------------------------------------------
QRect BottomSheet::sheetRect() const
{
    const int row_height = isPortrait() ? kRowHeight : landscapeCellHeight();

    int sheet_height = kHandleTopMargin + kHandleHeight + kVerticalPadding +
                       rowCount() * row_height + kVerticalPadding;

    // In portrait the title takes its own row above the list; in landscape it shares the buttons row.
    if (isPortrait() && !title_.isEmpty())
        sheet_height += kTitleHeight;

    return QRect(0, height() - sheet_height, width(), sheet_height);
}

//--------------------------------------------------------------------------------------------------
QRect BottomSheet::handleHitRect() const
{
    // Whole top strip above the items so the small visual handle is easy to hit.
    const QRect sheet = sheetRect();
    const int strip_height = kHandleTopMargin + kHandleHeight + kVerticalPadding;
    return QRect(sheet.left(), sheet.top(), sheet.width(), strip_height);
}

//--------------------------------------------------------------------------------------------------
int BottomSheet::itemsTop() const
{
    int top = sheetRect().top() + kHandleTopMargin + kHandleHeight + kVerticalPadding;
    if (isPortrait() && !title_.isEmpty())
        top += kTitleHeight;
    return top;
}

//--------------------------------------------------------------------------------------------------
int BottomSheet::columnCount() const
{
    if (isPortrait() || items_.isEmpty())
        return 1;

    const int available = width() - 2 * kHorizontalPadding - landscapeTitleWidth();
    const int columns = qMax(1, available / kCellWidth);
    return qMin(columns, int(items_.size()));
}

//--------------------------------------------------------------------------------------------------
int BottomSheet::rowCount() const
{
    const int columns = columnCount();
    return (int(items_.size()) + columns - 1) / columns;
}

//--------------------------------------------------------------------------------------------------
int BottomSheet::landscapeCellHeight() const
{
    const int line_height = QFontMetrics(Controls::scaledFont(font(), kCaptionFontScale)).height();
    return kCellIconSize + kCaptionTopMargin + kCaptionMaxLines * line_height + 2 * kVerticalPadding;
}

//--------------------------------------------------------------------------------------------------
int BottomSheet::landscapeTitleWidth() const
{
    if (title_.isEmpty())
        return 0;

    return fontMetrics().horizontalAdvance(title_) + kTitleSpacing;
}

//--------------------------------------------------------------------------------------------------
QRect BottomSheet::itemRect(int index) const
{
    const int columns = columnCount();
    const int row = index / columns;
    const int column = index % columns;

    if (isPortrait())
        return QRect(0, itemsTop() + row * kRowHeight, width(), kRowHeight);

    // The buttons are centered in the area left of the inline title (the whole width when there is
    // no title); the last row may hold fewer cells than the others.
    const int area_left = kHorizontalPadding + landscapeTitleWidth();
    const int area_width = width() - area_left - kHorizontalPadding;
    const int cells_in_row = qMin(columns, int(items_.size()) - row * columns);
    const int row_left = area_left + (area_width - cells_in_row * kCellWidth) / 2;
    const int cell_height = landscapeCellHeight();
    const int y = itemsTop() + row * cell_height;

    const int column_pos = (layoutDirection() == Qt::RightToLeft) ? (cells_in_row - 1 - column)
                                                                  : column;
    return QRect(row_left + column_pos * kCellWidth, y, kCellWidth, cell_height);
}

//--------------------------------------------------------------------------------------------------
int BottomSheet::itemAt(const QPoint& pos) const
{
    for (int i = 0; i < items_.size(); ++i)
    {
        if (itemRect(i).contains(pos))
            return i;
    }

    return -1;
}
