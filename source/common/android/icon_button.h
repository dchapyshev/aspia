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

#ifndef COMMON_ANDROID_ICON_BUTTON_H
#define COMMON_ANDROID_ICON_BUTTON_H

#include <QAbstractButton>
#include <QString>

// Icon-only button adapted for touch screens: an SVG icon centered in a large tap target with a
// circular press state layer. Keeps the QAbstractButton API (clicked signal etc.).
class IconButton final : public QAbstractButton
{
    Q_OBJECT

public:
    explicit IconButton(QWidget* parent = nullptr);
    explicit IconButton(const QString& icon_file_path, QWidget* parent = nullptr);
    ~IconButton() final;

    void setIconPath(const QString& icon_file_path);
    const QString& iconPath() const { return icon_file_path_; }

    // QAbstractButton implementation.
    QSize sizeHint() const final;

protected:
    // QAbstractButton implementation.
    void paintEvent(QPaintEvent* event) final;

private:
    QString icon_file_path_;

    Q_DISABLE_COPY_MOVE(IconButton)
};

#endif // COMMON_ANDROID_ICON_BUTTON_H
