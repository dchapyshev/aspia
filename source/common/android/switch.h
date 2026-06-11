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

#ifndef COMMON_ANDROID_SWITCH_H
#define COMMON_ANDROID_SWITCH_H

#include <QCheckBox>

class QVariantAnimation;

// On/off switch adapted for touch screens: a track with a sliding animated thumb, a press state
// layer and a large tap target. Keeps the QCheckBox API, so it is used the same way.
class Switch final : public QCheckBox
{
    Q_OBJECT

public:
    explicit Switch(QWidget* parent = nullptr);
    explicit Switch(const QString& text, QWidget* parent = nullptr);
    ~Switch() final;

    // QCheckBox implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;

protected:
    // QCheckBox implementation.
    void paintEvent(QPaintEvent* event) final;
    bool hitButton(const QPoint& pos) const final;

private slots:
    void onToggled(bool checked);

private:
    QVariantAnimation* animation_;
    double progress_;

    Q_DISABLE_COPY_MOVE(Switch)
};

#endif // COMMON_ANDROID_SWITCH_H
