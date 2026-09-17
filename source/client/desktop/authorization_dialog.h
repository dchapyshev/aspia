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

#ifndef CLIENT_DESKTOP_AUTHORIZATION_DIALOG_H
#define CLIENT_DESKTOP_AUTHORIZATION_DIALOG_H

#include <QDialog>
#include <QList>

#include <memory>

#include "client/config.h"

class QAbstractButton;

namespace Ui {
class AuthorizationDialog;
} // namespace Ui

class AuthorizationDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AuthorizationDialog(QWidget* parent = nullptr);
    ~AuthorizationDialog() final;

    void setOneTimePasswordEnabled(bool enable);

    void setSavedCredentials(const QList<CredentialConfig>& credentials);
    qint64 credentialId() const;

    void setSaveCredentialsVisible(bool visible);
    bool isSaveCredentialsChecked() const;

    QString userName() const;
    void setUserName(const QString& username);

    SecureString password() const;
    void setPassword(const SecureString& password);

protected:
    // QDialog implementation.
    void showEvent(QShowEvent* event) final;

private slots:
    void onModeToggled(bool checked);
    void onModeClicked(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void fitSize();

    bool isSaveCredentialsOffered() const;
    bool usesOneTimePassword() const;
    bool isOneTimePasswordOffered() const;
    bool usesSavedCredentials() const;
    bool hasSavedCredentials() const;
    const CredentialConfig* selectedCredential() const;
    void updateModes();

    std::unique_ptr<Ui::AuthorizationDialog> ui;
    QList<CredentialConfig> credentials_;
    bool one_time_password_enabled_ = false;
    bool one_time_password_choice_ = false;
    bool save_credentials_visible_ = false;
    Q_DISABLE_COPY_MOVE(AuthorizationDialog)
};

#endif // CLIENT_DESKTOP_AUTHORIZATION_DIALOG_H
