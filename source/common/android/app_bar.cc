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

#include "common/android/app_bar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include "common/android/controls.h"

namespace {

constexpr int kBarHeight = 56;
constexpr int kHorizontalPadding = 16;
constexpr int kBackWidth = 40;
constexpr int kBackArrowSize = 18;
constexpr double kTitleFontScale = 1.3;

} // namespace

//--------------------------------------------------------------------------------------------------
AppBar::AppBar(QWidget* parent)
    : QWidget(parent),
      back_visible_(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------------------------------
AppBar::~AppBar() = default;

//--------------------------------------------------------------------------------------------------
void AppBar::setTitle(const QString& title)
{
    if (title_ == title)
        return;

    title_ = title;
    update();
}

//--------------------------------------------------------------------------------------------------
void AppBar::setBackVisible(bool visible)
{
    if (back_visible_ == visible)
        return;

    back_visible_ = visible;
    update();
}

//--------------------------------------------------------------------------------------------------
void AppBar::setActions(const QList<QWidget*>& actions)
{
    for (QWidget* widget : actions_)
    {
        if (widget && !actions.contains(widget))
            widget->hide();
    }

    actions_ = actions;

    for (QWidget* widget : actions_)
    {
        widget->setParent(this);
        widget->show();
    }

    relayoutActions();
    update();
}

//--------------------------------------------------------------------------------------------------
QSize AppBar::sizeHint() const
{
    return QSize(0, kBarHeight);
}

//--------------------------------------------------------------------------------------------------
void AppBar::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool rtl = (layoutDirection() == Qt::RightToLeft);
    const QColor on_surface = palette().color(QPalette::WindowText);

    QRectF content = rect();
    content.adjust(kHorizontalPadding, 0, -kHorizontalPadding, 0);

    if (back_visible_)
    {
        // A chevron pointing towards the leading edge.
        const double cx = rtl ? content.right() - kBackArrowSize / 2.0
                              : content.left() + kBackArrowSize / 2.0;
        const double cy = content.center().y();
        const double half = kBackArrowSize / 2.0;

        QPolygonF arrow;
        if (rtl)
        {
            arrow << QPointF(cx - half / 2.0, cy - half)
                  << QPointF(cx + half / 2.0, cy)
                  << QPointF(cx - half / 2.0, cy + half);
        }
        else
        {
            arrow << QPointF(cx + half / 2.0, cy - half)
                  << QPointF(cx - half / 2.0, cy)
                  << QPointF(cx + half / 2.0, cy + half);
        }

        QPen pen(on_surface, 2);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(arrow);

        if (rtl)
            content.setRight(content.right() - kBackWidth);
        else
            content.setLeft(content.left() + kBackWidth);
    }

    // Reserve the trailing edge for the action buttons.
    const int actions_extent = actionsWidth();
    if (actions_extent > 0)
    {
        if (rtl)
            content.setLeft(content.left() + actions_extent);
        else
            content.setRight(content.right() - actions_extent);
    }

    QFont title_font = Controls::scaledFont(font(), kTitleFontScale);
    title_font.setWeight(QFont::DemiBold);

    const Qt::Alignment alignment = rtl ? Qt::AlignRight : Qt::AlignLeft;
    painter.setFont(title_font);
    painter.setPen(on_surface);
    painter.drawText(content, Qt::AlignVCenter | Qt::AlignAbsolute | alignment, title_);
}

//--------------------------------------------------------------------------------------------------
void AppBar::mousePressEvent(QMouseEvent* event)
{
    if (!back_visible_)
        return;

    const bool rtl = (layoutDirection() == Qt::RightToLeft);
    const int x = event->position().toPoint().x();
    const bool on_back = rtl ? (x >= width() - kHorizontalPadding - kBackWidth)
                             : (x <= kHorizontalPadding + kBackWidth);
    if (on_back)
        emit backClicked();
}

//--------------------------------------------------------------------------------------------------
void AppBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayoutActions();
}

//--------------------------------------------------------------------------------------------------
void AppBar::relayoutActions()
{
    const bool rtl = (layoutDirection() == Qt::RightToLeft);
    int x = rtl ? kHorizontalPadding : (width() - kHorizontalPadding - actionsWidth());

    for (QWidget* widget : actions_)
    {
        const QSize hint = widget->sizeHint();
        widget->setGeometry(x, (height() - hint.height()) / 2, hint.width(), hint.height());
        x += hint.width();
    }
}

//--------------------------------------------------------------------------------------------------
int AppBar::actionsWidth() const
{
    int total = 0;
    for (QWidget* widget : actions_)
        total += widget->sizeHint().width();
    return total;
}
