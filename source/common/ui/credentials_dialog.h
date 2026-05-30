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

#ifndef COMMON_UI_CREDENTIALS_DIALOG_H
#define COMMON_UI_CREDENTIALS_DIALOG_H

#include <QDialog>

#include <functional>
#include <memory>

class SecureString;

namespace Ui {
class CredentialsDialog;
} // namespace Ui

class CredentialsDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Type
    {
        ENTER_PASSWORD,    // one password field
        SET_PASSWORD,      // new + confirm
        CHANGE_PASSWORD,   // current + new + confirm
    };
    Q_ENUM(Type)

    explicit CredentialsDialog(Type type, QWidget* parent = nullptr);
    ~CredentialsDialog() final;

    // Header icon (left). Empty path hides the icon.
    void setHeaderIcon(const QString& svg_icon_path);

    // Header description text (right). Empty text hides the description.
    void setHeaderText(const QString& text);

    // Embeds a trailing toggle action in every password edit of the dialog.
    void setShowPasswordButtonVisible(bool visible);

    // Override the default label for each field.
    void setPasswordLabel(const QString& text);
    void setNewPasswordLabel(const QString& text);
    void setConfirmPasswordLabel(const QString& text);
    void setCurrentPasswordLabel(const QString& text);

    // Getters. Calling for a field not present in the current Type returns an empty value.
    SecureString currentPassword() const;
    SecureString password() const;
    SecureString confirmPassword() const;

    // Optional caller validator. Called on OK. Returning false keeps the dialog open
    // (the validator is responsible for showing an error to the user).
    using Validator = std::function<bool(CredentialsDialog*)>;
    void setValidator(Validator validator);

protected:
    // QDialog implementation.
    void showEvent(QShowEvent* event) final;

private:
    bool defaultValidate();
    void fitHeight();

    std::unique_ptr<Ui::CredentialsDialog> ui;
    const Type type_;
    Validator validator_;

    Q_DISABLE_COPY_MOVE(CredentialsDialog)
};

#endif // COMMON_UI_CREDENTIALS_DIALOG_H
