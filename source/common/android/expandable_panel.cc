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

#include "common/android/expandable_panel.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "common/android/controls.h"

namespace {

constexpr int kHeaderHeight = 56;
constexpr int kHeaderHMargin = 13;
constexpr int kHeaderSpacing = 12;

} // namespace

//--------------------------------------------------------------------------------------------------
ExpandablePanel::ExpandablePanel(QWidget* parent)
    : QWidget(parent),
      header_(new QWidget(this)),
      content_(new QWidget(this)),
      header_layout_(new QHBoxLayout(header_)),
      content_layout_(new QVBoxLayout(content_)),
      animation_(Controls::createAnimation(this))
{
    header_layout_->setContentsMargins(kHeaderHMargin, 0, kHeaderHMargin, 0);
    header_layout_->setSpacing(kHeaderSpacing);

    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(0);

    // The panel can shrink to nothing; it is kept hidden until the header is tapped.
    content_->setMinimumHeight(0);
    content_->setVisible(false);

    // Collapsed: the widget is only as tall as its header.
    setFixedHeight(kHeaderHeight);

    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        // The height is driven frame by frame; resizeEvent() keeps the header pinned and gives the
        // rest to the panel, so only the panel area grows or shrinks.
        setFixedHeight(value.toInt());
    });
    connect(animation_, &QVariantAnimation::finished, this, [this]()
    {
        if (expanded_)
        {
            // Hold the open height. Leaving it free lets the surrounding layout shrink the panel and
            // crush its rows together, so it is pinned to exactly what the content needs.
            setFixedHeight(kHeaderHeight + contentHeight());
        }
        else
        {
            content_->setVisible(false);
            setFixedHeight(kHeaderHeight);
        }
    });
}

//--------------------------------------------------------------------------------------------------
ExpandablePanel::~ExpandablePanel() = default;

//--------------------------------------------------------------------------------------------------
void ExpandablePanel::setExpanded(bool expanded)
{
    if (expanded_ == expanded)
        return;

    expanded_ = expanded;
    animation_->stop();

    if (expanded_)
    {
        content_->setVisible(true);
        animation_->setStartValue(height());
        animation_->setEndValue(kHeaderHeight + contentHeight());
    }
    else
    {
        animation_->setStartValue(height());
        animation_->setEndValue(kHeaderHeight);
    }

    animation_->start();
}

//--------------------------------------------------------------------------------------------------
void ExpandablePanel::contentChanged()
{
    // While collapsed there is nothing to resize, and while animating the running animation already
    // drives the height to its final value.
    if (!expanded_ || animation_->state() == QAbstractAnimation::Running)
        return;

    setFixedHeight(kHeaderHeight + contentHeight());
}

//--------------------------------------------------------------------------------------------------
QSize ExpandablePanel::sizeHint() const
{
    return QSize(kHeaderHeight, kHeaderHeight + (expanded_ ? contentHeight() : 0));
}

//--------------------------------------------------------------------------------------------------
void ExpandablePanel::paintEvent(QPaintEvent* /* event */)
{
    // A separator at the top of every item divides the list and sets the first item off from
    // whatever is above it.
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(0, 0, width(), 0);
}

//--------------------------------------------------------------------------------------------------
void ExpandablePanel::resizeEvent(QResizeEvent* /* event */)
{
    header_->setGeometry(0, 0, width(), kHeaderHeight);
    content_->setGeometry(0, kHeaderHeight, width(), qMax(0, height() - kHeaderHeight));
}

//--------------------------------------------------------------------------------------------------
void ExpandablePanel::mousePressEvent(QMouseEvent* event)
{
    // A plain widget ignores presses, and then no matching release is delivered. Accepting the press
    // on the header keeps the tap so mouseReleaseEvent() can report it.
    if (event->position().toPoint().y() < kHeaderHeight)
    {
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ExpandablePanel::mouseReleaseEvent(QMouseEvent* event)
{
    // A tap on the header reports a click; widgets placed in the header consume their own taps, so
    // they never reach here. Taps inside the open panel are left to its children.
    if (event->position().toPoint().y() < kHeaderHeight)
    {
        emit sig_headerClicked();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------------------------------
int ExpandablePanel::contentHeight() const
{
    const int content_width = width() > 0 ? width() : sizeHint().width();
    QLayout* layout = content_->layout();
    if (layout && layout->hasHeightForWidth())
        return layout->heightForWidth(content_width);
    return content_->sizeHint().height();
}
