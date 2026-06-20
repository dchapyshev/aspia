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

#ifndef COMMON_ANDROID_TAB_BAR_H
#define COMMON_ANDROID_TAB_BAR_H

#include <QStringList>
#include <QWidget>

// A row of text tabs sharing a top divider, with an accent indicator above the selected one. A flat,
// neutral switcher that blends into the surface it sits on rather than reading as a boxed control.
class TabBar final : public QWidget
{
    Q_OBJECT

public:
    explicit TabBar(QWidget* parent = nullptr);
    ~TabBar() final;

    void addTab(const QString& text);

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
    int tabAt(const QPoint& pos) const;

    QStringList tabs_;
    int current_ = 0;
    int pressed_ = -1;

    Q_DISABLE_COPY_MOVE(TabBar)
};

#endif // COMMON_ANDROID_TAB_BAR_H
