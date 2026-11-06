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

#include "common/desktop/date_edit.h"

#include <QAction>
#include <QCalendarWidget>
#include <QEvent>
#include <QPainter>
#include <QScreen>
#include <QStyle>

#include "base/gui_application.h"

namespace {

//--------------------------------------------------------------------------------------------------
// The icon is a single color glyph, painted in the text color so that it follows the theme.
QIcon calendarIcon(const QWidget* widget)
{
    const int size = widget->style()->pixelMetric(QStyle::PM_SmallIconSize);

    QPixmap pixmap = GuiApplication::svgPixmap(":/img/material/calendar_month.svg", QSize(size, size));
    if (pixmap.isNull())
        return QIcon();

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), widget->palette().color(QPalette::Text));
    return QIcon(pixmap);
}

} // namespace

//--------------------------------------------------------------------------------------------------
DateEdit::DateEdit(QWidget* parent)
    : QLineEdit(parent),
      date_(QDate::currentDate())
{
    setSizePolicy(QSizePolicy::Fixed, sizePolicy().verticalPolicy());
    setInputMethodHints(Qt::ImhDate);
    updateText();

    calendar_action_ = addAction(calendarIcon(this), TrailingPosition);

    connect(calendar_action_, &QAction::triggered, this, &DateEdit::onShowCalendar);
    connect(this, &QLineEdit::editingFinished, this, &DateEdit::onEditingFinished);
}

//--------------------------------------------------------------------------------------------------
DateEdit::~DateEdit() = default;

//--------------------------------------------------------------------------------------------------
QSize DateEdit::sizeHint() const
{
    // The base hint fits 17 characters of text, the longest date and the cursor are enough here.
    QSize hint = QLineEdit::sizeHint();

    const QFontMetrics fm(font());
    const QString sample = locale().toString(QDate(2000, 12, 28), QLocale::ShortFormat) + u' ';
    hint.rwidth() += fm.horizontalAdvance(sample) + 2 - fm.horizontalAdvance(u'x') * 17;
    return hint;
}

//--------------------------------------------------------------------------------------------------
QSize DateEdit::minimumSizeHint() const
{
    return sizeHint();
}

//--------------------------------------------------------------------------------------------------
void DateEdit::setDate(const QDate& date)
{
    if (!date.isValid() || date == date_)
        return;

    date_ = date;
    updateText();
    emit sig_dateChanged(date_);
}

//--------------------------------------------------------------------------------------------------
void DateEdit::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange ||
        event->type() == QEvent::ApplicationPaletteChange)
    {
        calendar_action_->setIcon(calendarIcon(this));
    }

    QLineEdit::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DateEdit::onShowCalendar()
{
    if (!calendar_)
    {
        calendar_ = new QCalendarWidget(this);
        calendar_->setWindowFlags(Qt::Popup);
        calendar_->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

        connect(calendar_, &QCalendarWidget::clicked, this, [this](const QDate& date)
        {
            calendar_->hide();
            setDate(date);
        });
        connect(calendar_, &QCalendarWidget::activated, this, [this](const QDate& date)
        {
            calendar_->hide();
            setDate(date);
        });
    }

    calendar_->setSelectedDate(date_);

    const bool rtl = layoutDirection() == Qt::RightToLeft;
    const QSize size = calendar_->sizeHint();
    const QRect screen_rect = screen()->availableGeometry();

    QPoint pos = mapToGlobal(rtl ? rect().bottomRight() : rect().bottomLeft());
    if (rtl)
        pos.rx() -= size.width();

    pos.setX(qBound(screen_rect.left(), pos.x(), screen_rect.right() - size.width()));
    if (pos.y() + size.height() > screen_rect.bottom())
        pos.setY(mapToGlobal(rect().topLeft()).y() - size.height());

    calendar_->move(pos);
    calendar_->show();
    calendar_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void DateEdit::onEditingFinished()
{
    const QDate date = locale().toDate(text(), QLocale::ShortFormat);
    if (date.isValid())
        setDate(date);

    // Either the new date or, for text that is not a date, the one the field had.
    updateText();
}

//--------------------------------------------------------------------------------------------------
void DateEdit::updateText()
{
    setText(locale().toString(date_, QLocale::ShortFormat));
}
