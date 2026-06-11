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

#include "common/android/scroll_indicator.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>

namespace {

constexpr int kWidth = 4;
constexpr int kMinHandle = 24;
constexpr int kVisibleMs = 900;
constexpr int kFadeMs = 300;
constexpr int kRadius = 2;
constexpr double kOpacity = 0.4;

} // namespace

//--------------------------------------------------------------------------------------------------
ScrollIndicator::ScrollIndicator(QAbstractScrollArea* area, int margin)
    : QWidget(area),
      area_(area),
      fade_(new QVariantAnimation(this)),
      hide_timer_(new QTimer(this)),
      margin_(margin),
      opacity_(0.0)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);

    fade_->setDuration(kFadeMs);
    fade_->setStartValue(1.0);
    fade_->setEndValue(0.0);
    connect(fade_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
    {
        opacity_ = value.toDouble();
        update();
    });

    hide_timer_->setSingleShot(true);
    hide_timer_->setInterval(kVisibleMs);
    connect(hide_timer_, &QTimer::timeout, fade_, [this]() { fade_->start(); });

    connect(area_->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &ScrollIndicator::onScrolled);

    area_->installEventFilter(this);
    updatePlacement();
}

//--------------------------------------------------------------------------------------------------
ScrollIndicator::~ScrollIndicator() = default;

//--------------------------------------------------------------------------------------------------
bool ScrollIndicator::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == area_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        updatePlacement();

    return QWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void ScrollIndicator::paintEvent(QPaintEvent* /* event */)
{
    if (opacity_ <= 0.0)
        return;

    const QScrollBar* scroll_bar = area_->verticalScrollBar();
    const int span = scroll_bar->maximum() - scroll_bar->minimum() + scroll_bar->pageStep();
    if (span <= scroll_bar->pageStep())
        return;

    const double track = height();
    const double handle_height = qMax<double>(kMinHandle, track * scroll_bar->pageStep() / span);
    const double handle_top = (track - handle_height) *
        (scroll_bar->value() - scroll_bar->minimum()) /
        qMax(1, scroll_bar->maximum() - scroll_bar->minimum());

    QColor color = palette().color(QPalette::WindowText);
    color.setAlphaF(kOpacity * opacity_);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(0, handle_top, width(), handle_height), kRadius, kRadius);
}

//--------------------------------------------------------------------------------------------------
void ScrollIndicator::onScrolled()
{
    fade_->stop();
    opacity_ = 1.0;
    update();
    hide_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void ScrollIndicator::updatePlacement()
{
    const QRect area = area_->rect();
    setGeometry(area.right() - kWidth - margin_, area.top() + margin_, kWidth,
                area.height() - margin_ * 2);
    raise();
}
