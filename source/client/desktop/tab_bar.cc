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

#include "client/desktop/tab_bar.h"

#include <QApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

namespace {

// Vertical distance the cursor must travel beyond the tab bar bounds while dragging a tab to
// trigger detachment. Horizontal movement keeps the tab in the bar (reorder).
constexpr int kDetachThresholdPx = 30;

// Drop-target pulsing animation parameters.
constexpr MilliSeconds kPulseTick{ 33 };
constexpr MilliSeconds kPulsePeriod{ 800 };
constexpr int kPulseMaxAlpha = 220;
constexpr int kPulseFrameWidthPx = 2;

} // namespace

//--------------------------------------------------------------------------------------------------
TabBar::TabBar(QWidget* parent)
    : QTabBar(parent),
      pulse_timer_(new QTimer(this))
{
    pulse_timer_->setInterval(kPulseTick);
    connect(pulse_timer_, &QTimer::timeout, this, &TabBar::onPulseTick);
}

//--------------------------------------------------------------------------------------------------
TabBar::~TabBar() = default;

//--------------------------------------------------------------------------------------------------
void TabBar::setDropTarget(int index)
{
    if (drop_target_index_ == index)
        return;

    drop_target_index_ = index;

    if (index >= 0)
    {
        pulse_phase_ = MilliSeconds::zero();
        if (!pulse_timer_->isActive())
            pulse_timer_->start();
    }
    else
    {
        pulse_timer_->stop();
    }

    update();
}

//--------------------------------------------------------------------------------------------------
void TabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        pressed_tab_index_ = tabAt(event->pos());

    QTabBar::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TabBar::mouseMoveEvent(QMouseEvent* event)
{
    QTabBar::mouseMoveEvent(event);

    if (!(event->buttons() & Qt::LeftButton) || pressed_tab_index_ < 0)
        return;

    int y = event->position().toPoint().y();
    if (y >= -kDetachThresholdPx && y <= height() + kDetachThresholdPx)
        return;

    int detach_index = pressed_tab_index_;
    pressed_tab_index_ = -1;

    // Cancel QTabBar's internal drag state so the synthetic release leaves it in a clean state.
    QMouseEvent release(QEvent::MouseButtonRelease,
                        event->position(),
                        event->scenePosition(),
                        event->globalPosition(),
                        Qt::LeftButton,
                        Qt::NoButton,
                        event->modifiers());
    QTabBar::mouseReleaseEvent(&release);

    emit sig_tabDetachRequested(detach_index, event->globalPosition().toPoint());
}

//--------------------------------------------------------------------------------------------------
void TabBar::mouseReleaseEvent(QMouseEvent* event)
{
    pressed_tab_index_ = -1;
    QTabBar::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TabBar::paintEvent(QPaintEvent* event)
{
    QTabBar::paintEvent(event);

    if (drop_target_index_ < 0 || drop_target_index_ >= count())
        return;

    if (!isTabVisible(drop_target_index_))
        return;

    QRect rect = tabRect(drop_target_index_);
    if (rect.isEmpty())
        return;

    // Triangle wave: 0 -> max -> 0 over kPulsePeriod.
    double t = static_cast<double>(pulse_phase_.count()) / kPulsePeriod.count();
    double wave = (t < 0.5) ? (t * 2.0) : ((1.0 - t) * 2.0);
    int alpha = static_cast<int>(wave * kPulseMaxAlpha);

    QColor color = palette().highlight().color();
    color.setAlpha(alpha);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, kPulseFrameWidthPx));

    int inset = kPulseFrameWidthPx / 2 + 1;
    painter.drawRect(rect.adjusted(inset, inset, -inset, -inset));
}

//--------------------------------------------------------------------------------------------------
void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);

    // Replace Qt's built-in close button.
    QToolButton* close_button = new QToolButton(this);
    close_button->setIcon(QIcon(":/img/close-window.svg"));
    close_button->setIconSize(QSize(16, 16));
    close_button->setFixedSize(QSize(16, 16));
    close_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    close_button->setAutoRaise(true);
    close_button->setCursor(Qt::ArrowCursor);
    close_button->setFocusPolicy(Qt::NoFocus);
    close_button->setStyleSheet(
        "QToolButton { border: none; margin: 0px; padding: 0px; }"
        "QToolButton:hover { background-color: rgba(0, 0, 0, 40); border-radius: 2px; }"
        "QToolButton:pressed { background-color: rgba(0, 0, 0, 80); }");

    connect(close_button, &QToolButton::clicked, this, [this, close_button]()
    {
        for (int i = 0; i < count(); ++i)
        {
            if (tabButton(i, QTabBar::RightSide) == close_button ||
                tabButton(i, QTabBar::LeftSide) == close_button)
            {
                emit tabCloseRequested(i);
                break;
            }
        }
    });

    QTabBar::ButtonPosition side = static_cast<QTabBar::ButtonPosition>(
        style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));
    setTabButton(index, side, close_button);
}

//--------------------------------------------------------------------------------------------------
void TabBar::onPulseTick()
{
    pulse_phase_ = (pulse_phase_ + kPulseTick) % kPulsePeriod;
    update();
}
