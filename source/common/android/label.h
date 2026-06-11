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

#ifndef COMMON_ANDROID_LABEL_H
#define COMMON_ANDROID_LABEL_H

#include <QLabel>

// QLabel adapted for touch screens: the role selects a font size and color suitable for mobile
// layouts. Keeps the QLabel API, so it is used the same way.
class Label final : public QLabel
{
    Q_OBJECT

public:
    enum class Role
    {
        TITLE,  // Prominent screen and section titles.
        BODY,   // Regular text.
        CAPTION // Muted secondary text.
    };

    explicit Label(QWidget* parent = nullptr);
    explicit Label(const QString& text, QWidget* parent = nullptr);
    Label(const QString& text, Role role, QWidget* parent = nullptr);
    ~Label() final;

    void setRole(Role role);
    Role role() const { return role_; }

protected:
    // QLabel implementation.
    void changeEvent(QEvent* event) final;

private:
    void applyRole();

    QFont base_font_;
    Role role_;
    bool applying_palette_;

    Q_DISABLE_COPY_MOVE(Label)
};

#endif // COMMON_ANDROID_LABEL_H
