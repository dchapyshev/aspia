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

#ifndef COMMON_ANDROID_LINE_EDIT_H
#define COMMON_ANDROID_LINE_EDIT_H

#include <QLineEdit>

class QVariantAnimation;

// QLineEdit adapted for touch screens: a taller field with a rounded outline that highlights on
// focus and a floating label that moves from inside the field onto the outline.
class LineEdit final : public QLineEdit
{
    Q_OBJECT

public:
    explicit LineEdit(QWidget* parent = nullptr);
    ~LineEdit() final;

    void setLabel(const QString& label);
    const QString& label() const { return label_; }

    // QLineEdit implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;

protected:
    // QLineEdit implementation.
    void paintEvent(QPaintEvent* event) final;
    void focusInEvent(QFocusEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;

private slots:
    void onTextChanged(const QString& text);

private:
    int labelOverflow() const;
    void updateFloatState();

    QVariantAnimation* float_animation_;
    QVariantAnimation* focus_animation_;
    double float_progress_;
    double focus_progress_;
    QString label_;

    Q_DISABLE_COPY_MOVE(LineEdit)
};

#endif // COMMON_ANDROID_LINE_EDIT_H
