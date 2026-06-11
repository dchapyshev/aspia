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

#include "common/android/icon_button.h"

#include <QPainter>

#include "base/gui_application.h"

namespace {

constexpr int kButtonSize = 48;
constexpr int kIconSize = 24;
constexpr int kStateLayerSize = 40;
constexpr double kDisabledOpacity = 0.38;
constexpr double kStateLayerOpacity = 0.12;

} // namespace

//--------------------------------------------------------------------------------------------------
IconButton::IconButton(QWidget* parent)
    : IconButton(QString(), parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
IconButton::IconButton(const QString& icon_file_path, QWidget* parent)
    : QAbstractButton(parent),
      icon_file_path_(icon_file_path)
{
    setCursor(Qt::PointingHandCursor);
}

//--------------------------------------------------------------------------------------------------
IconButton::~IconButton() = default;

//--------------------------------------------------------------------------------------------------
void IconButton::setIconPath(const QString& icon_file_path)
{
    if (icon_file_path_ == icon_file_path)
        return;

    icon_file_path_ = icon_file_path;
    update();
}

//--------------------------------------------------------------------------------------------------
QSize IconButton::sizeHint() const
{
    return QSize(kButtonSize, kButtonSize);
}

//--------------------------------------------------------------------------------------------------
void IconButton::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!isEnabled())
        painter.setOpacity(kDisabledOpacity);

    if (isDown())
    {
        QColor layer = palette().color(QPalette::Highlight);
        layer.setAlphaF(kStateLayerOpacity);

        painter.setPen(Qt::NoPen);
        painter.setBrush(layer);
        painter.drawEllipse(QPointF(rect().center()), kStateLayerSize / 2.0, kStateLayerSize / 2.0);
    }

    if (!icon_file_path_.isEmpty())
    {
        const qreal dpr = devicePixelRatioF();
        const int icon_px = qRound(kIconSize * dpr);

        QPixmap icon = GuiApplication::svgPixmap(icon_file_path_, QSize(icon_px, icon_px));
        icon.setDevicePixelRatio(dpr);
        painter.drawPixmap(QPointF(rect().center().x() - kIconSize / 2.0,
                                   rect().center().y() - kIconSize / 2.0), icon);
    }
}
