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

#ifndef COMMON_ANDROID_SEGMENTED_BUTTON_H
#define COMMON_ANDROID_SEGMENTED_BUTTON_H

#include <QStringList>
#include <QWidget>

// A row of mutually exclusive segments sharing a single outline, with the selected one filled. Used
// as a touch-friendly tab switcher; reads as one control split in parts rather than separate buttons.
class SegmentedButton final : public QWidget
{
    Q_OBJECT

public:
    explicit SegmentedButton(QWidget* parent = nullptr);
    ~SegmentedButton() final;

    void addSegment(const QString& text);

    void setCurrentIndex(int index);
    int currentIndex() const { return current_; }

    // QWidget implementation.
    QSize sizeHint() const final;

signals:
    void sig_currentChanged(int index);

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;

private:
    int segmentAt(const QPoint& pos) const;

    QStringList segments_;
    int current_ = 0;
    int pressed_ = -1;

    Q_DISABLE_COPY_MOVE(SegmentedButton)
};

#endif // COMMON_ANDROID_SEGMENTED_BUTTON_H
