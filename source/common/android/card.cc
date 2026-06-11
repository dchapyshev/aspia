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

#include "common/android/card.h"

#include <QPainter>
#include <QVBoxLayout>

namespace {

constexpr int kCornerRadius = 12;
constexpr int kPadding = 16;
constexpr int kSpacing = 12;
constexpr int kOutlineWidth = 1;

} // namespace

//--------------------------------------------------------------------------------------------------
Card::Card(QWidget* parent)
    : Card(Role::FILLED, parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
Card::Card(Role role, QWidget* parent)
    : QWidget(parent),
      content_layout_(new QVBoxLayout(this)),
      role_(role)
{
    content_layout_->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    content_layout_->setSpacing(kSpacing);
}

//--------------------------------------------------------------------------------------------------
Card::~Card() = default;

//--------------------------------------------------------------------------------------------------
void Card::setRole(Role role)
{
    if (role_ == role)
        return;

    role_ = role;
    update();
}

//--------------------------------------------------------------------------------------------------
void Card::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF surface = rect();

    if (role_ == Role::FILLED)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Base));
    }
    else
    {
        surface.adjust(kOutlineWidth / 2.0, kOutlineWidth / 2.0,
                       -kOutlineWidth / 2.0, -kOutlineWidth / 2.0);
        painter.setPen(QPen(palette().color(QPalette::Mid), kOutlineWidth));
        painter.setBrush(Qt::NoBrush);
    }

    painter.drawRoundedRect(surface, kCornerRadius, kCornerRadius);
}
