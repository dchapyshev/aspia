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

#include "common/android/bottom_navigation_bar.h"

#include <QMouseEvent>
#include <QPainter>

#include "base/gui_application.h"
#include "common/android/controls.h"

namespace {

constexpr int kBarHeight = 64;
constexpr int kIconSize = 24;
constexpr int kPillWidth = 64;
constexpr int kPillHeight = 32;
constexpr int kIconTextSpacing = 4;
constexpr double kLabelFontScale = 0.85;
constexpr double kInactiveOpacity = 0.6;
constexpr double kPillOpacity = 0.14;

} // namespace

//--------------------------------------------------------------------------------------------------
BottomNavigationBar::BottomNavigationBar(QWidget* parent)
    : QWidget(parent),
      current_(0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------------------------------
BottomNavigationBar::~BottomNavigationBar() = default;

//--------------------------------------------------------------------------------------------------
int BottomNavigationBar::addItem(const QString& text, const QString& icon_file_path)
{
    items_.append({text, icon_file_path});
    update();
    return items_.size() - 1;
}

//--------------------------------------------------------------------------------------------------
void BottomNavigationBar::setItemText(int index, const QString& text)
{
    if (index < 0 || index >= items_.size())
        return;

    items_[index].text = text;
    update();
}

//--------------------------------------------------------------------------------------------------
void BottomNavigationBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= items_.size() || index == current_)
        return;

    current_ = index;
    update();
    emit currentChanged(current_);
}

//--------------------------------------------------------------------------------------------------
QSize BottomNavigationBar::sizeHint() const
{
    return QSize(0, kBarHeight);
}

//--------------------------------------------------------------------------------------------------
void BottomNavigationBar::paintEvent(QPaintEvent* /* event */)
{
    if (items_.isEmpty())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor accent = palette().color(QPalette::Highlight);
    const QColor on_surface = palette().color(QPalette::WindowText);

    // A hairline divider separates the bar from the content above it.
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(0, 0, width(), 0);

    const QFont label_font = Controls::scaledFont(font(), kLabelFontScale);
    const QFontMetricsF label_metrics(label_font);
    const double cell_width = static_cast<double>(width()) / items_.size();

    for (int i = 0; i < items_.size(); ++i)
    {
        const Item& item = items_[i];
        const bool active = (i == current_);
        const QRectF cell(i * cell_width, 0, cell_width, height());

        QColor color = active ? accent : on_surface;
        if (!active)
            color.setAlphaF(kInactiveOpacity);

        const double content_height = kIconSize + kIconTextSpacing + label_metrics.height();
        double top = cell.center().y() - content_height / 2.0;

        const QRectF pill(cell.center().x() - kPillWidth / 2.0, top - (kPillHeight - kIconSize) / 2.0,
                          kPillWidth, kPillHeight);
        if (active)
        {
            QColor pill_color = accent;
            pill_color.setAlphaF(kPillOpacity);
            painter.setPen(Qt::NoPen);
            painter.setBrush(pill_color);
            painter.drawRoundedRect(pill, kPillHeight / 2.0, kPillHeight / 2.0);
        }

        if (!item.icon_file_path.isEmpty())
        {
            QPixmap icon = GuiApplication::svgPixmap(item.icon_file_path, QSize(kIconSize, kIconSize));
            painter.drawPixmap(QPointF(cell.center().x() - kIconSize / 2.0, top), icon);
        }

        const QRectF text_rect(cell.left(), top + kIconSize + kIconTextSpacing, cell.width(),
                               label_metrics.height());
        painter.setFont(label_font);
        painter.setPen(color);
        painter.drawText(text_rect, Qt::AlignHCenter | Qt::AlignVCenter, item.text);
    }
}

//--------------------------------------------------------------------------------------------------
void BottomNavigationBar::mousePressEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position().toPoint());
    if (index >= 0)
        setCurrentIndex(index);
}

//--------------------------------------------------------------------------------------------------
int BottomNavigationBar::indexAt(const QPoint& pos) const
{
    if (items_.isEmpty())
        return -1;

    const int index = pos.x() * items_.size() / width();
    return qBound(0, index, items_.size() - 1);
}
