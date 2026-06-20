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

#ifndef COMMON_ANDROID_COMBO_BOX_H
#define COMMON_ANDROID_COMBO_BOX_H

#include <QComboBox>

class QVariantAnimation;

// QComboBox adapted for touch screens: a taller field with a rounded outline that highlights on
// focus, a label resting on the outline and a drop-down indicator. Keeps the QComboBox API, so it
// is used the same way.
class ComboBox final : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel)

public:
    explicit ComboBox(QWidget* parent = nullptr);
    ~ComboBox() final;

    void setLabel(const QString& label);
    const QString& label() const { return label_; }

    // Marks the item at |index| as nested under the preceding top-level item (drawn with an indent).
    void setItemIndented(int index);

    // QComboBox implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;

protected:
    // QComboBox implementation.
    void paintEvent(QPaintEvent* event) final;
    void focusInEvent(QFocusEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;

private:
    int labelOverflow() const;

    QVariantAnimation* focus_animation_;
    double focus_progress_;
    QString label_;

    Q_DISABLE_COPY_MOVE(ComboBox)
};

#endif // COMMON_ANDROID_COMBO_BOX_H
