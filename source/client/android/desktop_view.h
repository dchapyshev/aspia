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

#ifndef CLIENT_ANDROID_DESKTOP_VIEW_H
#define CLIENT_ANDROID_DESKTOP_VIEW_H

#include <QImage>
#include <QWidget>

#include <memory>

class Frame;

class DesktopView final : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopView(QWidget* parent = nullptr);
    ~DesktopView() final;

    void setFrame(std::shared_ptr<Frame> frame);
    void refresh();

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;

private:
    std::shared_ptr<Frame> frame_;
    QImage image_;

    Q_DISABLE_COPY_MOVE(DesktopView)
};

#endif // CLIENT_ANDROID_DESKTOP_VIEW_H
