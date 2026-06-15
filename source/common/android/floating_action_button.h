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

#ifndef COMMON_ANDROID_FLOATING_ACTION_BUTTON_H
#define COMMON_ANDROID_FLOATING_ACTION_BUTTON_H

#include <QPoint>
#include <QString>
#include <QWidget>

// Circular action button that floats over its parent. It can be dragged to any position inside the
// parent and emits sig_clicked() on a tap that does not turn into a drag.
class FloatingActionButton final : public QWidget
{
    Q_OBJECT

public:
    explicit FloatingActionButton(const QString& icon_file_path, QWidget* parent = nullptr);
    ~FloatingActionButton() final;

signals:
    void sig_clicked();

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;

private:
    QString icon_file_path_;
    QPoint press_offset_;
    QPoint press_global_;
    bool pressed_ = false;
    bool dragging_ = false;

    Q_DISABLE_COPY_MOVE(FloatingActionButton)
};

#endif // COMMON_ANDROID_FLOATING_ACTION_BUTTON_H
