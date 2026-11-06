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

#ifndef COMMON_DESKTOP_DATE_EDIT_H
#define COMMON_DESKTOP_DATE_EDIT_H

#include <QDate>
#include <QLineEdit>

class QAction;
class QCalendarWidget;

// Line edit that holds a date and opens a calendar from a button inside the field.
class DateEdit final : public QLineEdit
{
    Q_OBJECT

public:
    explicit DateEdit(QWidget* parent = nullptr);
    ~DateEdit() final;

    QDate date() const { return date_; }

    // QLineEdit implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;

public slots:
    void setDate(const QDate& date);

signals:
    void sig_dateChanged(const QDate& date);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onShowCalendar();
    void onEditingFinished();

private:
    void updateText();

    QDate date_;
    QAction* calendar_action_ = nullptr;
    QCalendarWidget* calendar_ = nullptr;

    Q_DISABLE_COPY_MOVE(DateEdit)
};

#endif // COMMON_DESKTOP_DATE_EDIT_H
