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

#include "common/desktop/icon_text_button.h"

#include <QStyleOptionToolButton>
#include <QStylePainter>

namespace {

constexpr int kIconTextGap = 4;
constexpr int kIconTextHPadding = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
IconTextButton::IconTextButton(QWidget* parent)
    : QToolButton(parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
IconTextButton::~IconTextButton() = default;

//--------------------------------------------------------------------------------------------------
void IconTextButton::setIconOnRight(bool on_right)
{
    if (icon_on_right_ == on_right)
        return;

    icon_on_right_ = on_right;
    updateGeometry();
    update();
}

//--------------------------------------------------------------------------------------------------
QSize IconTextButton::sizeHint() const
{
    QFontMetrics fm(font());
    int text_w = fm.horizontalAdvance(text());
    int icon_w = iconSize().width();
    int icon_h = iconSize().height();
    int w = text_w + icon_w + kIconTextGap + kIconTextHPadding * 2;
    int h = qMax(fm.height(), icon_h) + kIconTextHPadding;
    return QSize(w, h);
}

//--------------------------------------------------------------------------------------------------
void IconTextButton::paintEvent(QPaintEvent* /* event */)
{
    QStylePainter p(this);
    QStyleOptionToolButton opt;
    initStyleOption(&opt);

    QStyleOptionToolButton bg_opt = opt;
    bg_opt.text.clear();
    bg_opt.icon = QIcon();
    bg_opt.toolButtonStyle = Qt::ToolButtonIconOnly;
    p.drawComplexControl(QStyle::CC_ToolButton, bg_opt);

    QFontMetrics fm(font());
    QString btn_text = text();
    int text_w = fm.horizontalAdvance(btn_text);
    int icon_w = iconSize().width();
    int icon_h = iconSize().height();
    int total_w = text_w + icon_w + kIconTextGap;
    int x = (width() - total_w) / 2;
    int y_offset = 0;

    if (opt.state & QStyle::State_Sunken)
    {
        x += style()->pixelMetric(QStyle::PM_ButtonShiftHorizontal, &opt, this);
        y_offset = style()->pixelMetric(QStyle::PM_ButtonShiftVertical, &opt, this);
    }

    int text_baseline = (height() + fm.ascent() - fm.descent()) / 2 + y_offset;
    int icon_y = (height() - icon_h) / 2 + y_offset;

    QIcon::Mode icon_mode = QIcon::Normal;
    if (!isEnabled())
        icon_mode = QIcon::Disabled;
    else if (opt.state & QStyle::State_Sunken)
        icon_mode = QIcon::Active;

    QPixmap pix = icon().pixmap(QSize(icon_w, icon_h), icon_mode);

    QColor text_color = palette().color(
        isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);

    if (icon_on_right_)
    {
        p.setPen(text_color);
        p.drawText(x, text_baseline, btn_text);
        x += text_w + kIconTextGap;
        p.drawPixmap(x, icon_y, pix);
    }
    else
    {
        p.drawPixmap(x, icon_y, pix);
        x += icon_w + kIconTextGap;
        p.setPen(text_color);
        p.drawText(x, text_baseline, btn_text);
    }
}
