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

#ifndef COMMON_ANDROID_RADIO_BUTTON_H
#define COMMON_ANDROID_RADIO_BUTTON_H

#include <QRadioButton>

class QVariantAnimation;

// Radio button adapted for touch screens: an outlined ring with an animated inner dot, a press
// state layer and a large tap target. Keeps the QRadioButton API, so it is used the same way.
class RadioButton final : public QRadioButton
{
    Q_OBJECT

public:
    explicit RadioButton(QWidget* parent = nullptr);
    explicit RadioButton(const QString& text, QWidget* parent = nullptr);
    ~RadioButton() final;

    // QRadioButton implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;
    int heightForWidth(int width) const final;

protected:
    // QRadioButton implementation.
    void paintEvent(QPaintEvent* event) final;
    bool hitButton(const QPoint& pos) const final;

private slots:
    void onToggled(bool checked);

private:
    QVariantAnimation* animation_;
    double progress_;

    Q_DISABLE_COPY_MOVE(RadioButton)
};

#endif // COMMON_ANDROID_RADIO_BUTTON_H
