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

#include "common/android/menu.h"

#include <QMouseEvent>
#include <QPainter>

#include "common/android/controls.h"

namespace {

constexpr int kItemHeight = 48;
constexpr int kHorizontalPadding = 16;
constexpr int kVerticalPadding = 8;
constexpr int kIconSize = 24;
constexpr int kIconSpacing = 12;
constexpr int kCornerRadius = 12;
constexpr int kMinWidth = 200;
constexpr int kMargin = 8;
constexpr int kShadowLayers = 8;
constexpr double kOutlineWidth = 1.0;
constexpr double kPressLayerOpacity = 0.12;

} // namespace

//--------------------------------------------------------------------------------------------------
Menu::Menu(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setFont(Controls::scaledFont(font(), Controls::kFontScale));
}

//--------------------------------------------------------------------------------------------------
Menu::~Menu() = default;

//--------------------------------------------------------------------------------------------------
void Menu::addItem(const QString& text, const QIcon& icon)
{
    items_.append({ icon, text });
}

//--------------------------------------------------------------------------------------------------
void Menu::popup(const QRect& anchor)
{
    // Cover the whole top-level window so a tap anywhere outside the surface is caught and closes it.
    QWidget* host = parentWidget() ? parentWidget()->window() : nullptr;
    if (!host)
        return;

    setParent(host);
    setGeometry(host->rect());

    const QRect local(host->mapFromGlobal(anchor.topLeft()), anchor.size());
    const QSize surface = surfaceSize();

    // Drop from the anchor's near edge: its right edge in LTR, its left edge in RTL.
    const int x = (layoutDirection() == Qt::RightToLeft) ? local.left()
                                                         : local.right() - surface.width();

    surface_pos_ = QPoint(qBound(kMargin, x, host->width() - surface.width() - kMargin),
                          qBound(kMargin, local.bottom(), host->height() - surface.height() - kMargin));

    raise();
    show();
}

//--------------------------------------------------------------------------------------------------
void Menu::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect surface = surfaceRect();

    // A soft drop shadow built from stacked translucent rounded rects.
    painter.setPen(Qt::NoPen);
    for (int i = kShadowLayers; i > 0; --i)
    {
        QColor shadow(0, 0, 0);
        shadow.setAlphaF(0.015);
        painter.setBrush(shadow);
        painter.drawRoundedRect(surface.adjusted(-i, -i + 2, i, i + 2),
                                kCornerRadius + i, kCornerRadius + i);
    }

    painter.setPen(QPen(palette().color(QPalette::Mid), kOutlineWidth));
    painter.setBrush(palette().color(QPalette::Base));
    painter.drawRoundedRect(QRectF(surface).adjusted(kOutlineWidth / 2.0, kOutlineWidth / 2.0,
                                                     -kOutlineWidth / 2.0, -kOutlineWidth / 2.0),
                            kCornerRadius, kCornerRadius);

    const QRect items_area = surface.adjusted(0, kVerticalPadding, 0, -kVerticalPadding);

    const bool rtl = (layoutDirection() == Qt::RightToLeft);

    // Reserve a leading icon column (on the start side) when any item has an icon, so text aligns.
    const bool icons = hasIcons();
    const int icon_margin = kHorizontalPadding + (icons ? kIconSize + kIconSpacing : 0);

    for (int i = 0; i < items_.size(); ++i)
    {
        const QRect row(items_area.left(), items_area.top() + i * kItemHeight,
                        items_area.width(), kItemHeight);

        if (i == active_)
        {
            QColor layer = palette().color(QPalette::Highlight);
            layer.setAlphaF(kPressLayerOpacity);
            painter.fillRect(row, layer);
        }

        if (!items_[i].icon.isNull())
        {
            const int icon_x = rtl ? row.right() - kHorizontalPadding - kIconSize + 1
                                   : row.left() + kHorizontalPadding;
            items_[i].icon.paint(&painter, QRect(icon_x, row.center().y() - kIconSize / 2,
                                                 kIconSize, kIconSize));
        }

        const QRect text_rect = rtl ? row.adjusted(kHorizontalPadding, 0, -icon_margin, 0)
                                    : row.adjusted(icon_margin, 0, -kHorizontalPadding, 0);

        // AlignAbsolute keeps the requested side fixed; QPainter would otherwise mirror
        // AlignLeft/AlignRight under an RTL layout.
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(text_rect,
                         Qt::AlignVCenter | Qt::AlignAbsolute | (rtl ? Qt::AlignRight : Qt::AlignLeft),
                         items_[i].text);
    }
}

//--------------------------------------------------------------------------------------------------
void Menu::mouseMoveEvent(QMouseEvent* event)
{
    const int item = itemAt(event->position().toPoint());
    if (item == active_)
        return;

    active_ = item;
    update();
}

//--------------------------------------------------------------------------------------------------
void Menu::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if (!surfaceRect().contains(pos))
    {
        close();
        return;
    }

    active_ = itemAt(pos);
    update();
}

//--------------------------------------------------------------------------------------------------
void Menu::mouseReleaseEvent(QMouseEvent* event)
{
    const int item = itemAt(event->position().toPoint());
    if (item >= 0)
    {
        emit sig_triggered(item);
        close();
    }
}

//--------------------------------------------------------------------------------------------------
QRect Menu::surfaceRect() const
{
    return QRect(surface_pos_, surfaceSize());
}

//--------------------------------------------------------------------------------------------------
QSize Menu::surfaceSize() const
{
    const int icon_column = hasIcons() ? kIconSize + kIconSpacing : 0;

    int width = kMinWidth;
    for (const Item& item : items_)
    {
        width = qMax(width, fontMetrics().horizontalAdvance(item.text) + icon_column +
                            2 * kHorizontalPadding);
    }

    return QSize(width, items_.size() * kItemHeight + 2 * kVerticalPadding);
}

//--------------------------------------------------------------------------------------------------
int Menu::itemAt(const QPoint& pos) const
{
    const QRect items_area = surfaceRect().adjusted(0, kVerticalPadding, 0, -kVerticalPadding);
    if (!items_area.contains(pos))
        return -1;

    const int index = (pos.y() - items_area.top()) / kItemHeight;
    return (index >= 0 && index < items_.size()) ? index : -1;
}

//--------------------------------------------------------------------------------------------------
bool Menu::hasIcons() const
{
    for (const Item& item : items_)
    {
        if (!item.icon.isNull())
            return true;
    }

    return false;
}
