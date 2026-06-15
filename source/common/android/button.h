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

#ifndef COMMON_ANDROID_BUTTON_H
#define COMMON_ANDROID_BUTTON_H

#include <QColor>
#include <QPushButton>

class QVariantAnimation;

// QPushButton adapted for touch screens: a pill-shaped button with an animated press state layer
// and a large tap target. Keeps the QPushButton API, so it is used the same way.
class Button final : public QPushButton
{
    Q_OBJECT

public:
    enum class Role
    {
        FILLED, // Primary action: an accent colored surface with contrasting text.
        TEXT    // Secondary action: no surface, accent colored text.
    };

    explicit Button(QWidget* parent = nullptr);
    explicit Button(const QString& text, QWidget* parent = nullptr);
    Button(const QString& text, Role role, QWidget* parent = nullptr);
    ~Button() final;

    void setRole(Role role);
    Role role() const { return role_; }

    // Overrides the brand accent for this button, for destructive actions tinted with a different
    // color. An invalid color falls back to the shared brand accent.
    void setAccentColor(const QColor& color);

    // QPushButton implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;

protected:
    // QPushButton implementation.
    void paintEvent(QPaintEvent* event) final;

private slots:
    void onPressed();
    void onReleased();

private:
    QVariantAnimation* animation_;
    double press_progress_;
    Role role_;
    QColor accent_color_;

    Q_DISABLE_COPY_MOVE(Button)
};

#endif // COMMON_ANDROID_BUTTON_H
