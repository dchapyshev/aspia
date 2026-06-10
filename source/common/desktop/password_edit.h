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

#ifndef COMMON_DESKTOP_PASSWORD_EDIT_H
#define COMMON_DESKTOP_PASSWORD_EDIT_H

#include <QLineEdit>
#include <QPointer>

class QToolButton;
class SecureString;

class PasswordEdit final : public QLineEdit
{
    Q_OBJECT

public:
    explicit PasswordEdit(QWidget* parent = nullptr);
    ~PasswordEdit() final;

    void setShowPassword(bool enable);
    bool isShowPassword() const;

    // Embeds a trailing toggle action (eye icon) inside the line edit. Removes the need
    // for an external QPushButton in dialogs.
    void setShowPasswordButtonVisible(bool visible);

    void setPassword(const SecureString& password);
    SecureString password() const;

    // Use setPassword()/password() instead. Shadowing the base methods catches direct misuse
    // through PasswordEdit*; access through a QLineEdit* upcast still bypasses this (these
    // members are not virtual in Qt).
    void setText(const QString& text) = delete;
    QString text() const = delete;

public slots:
    // Overwrites the current text with zeros in place before calling QLineEdit::clear(),
    // reducing the window during which the password remains in the widget's text buffer.
    void clear();

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;

private:
    QPointer<QToolButton> show_password_button_;

    Q_DISABLE_COPY_MOVE(PasswordEdit)
};

#endif // COMMON_DESKTOP_PASSWORD_EDIT_H
