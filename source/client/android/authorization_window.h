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

#ifndef CLIENT_ANDROID_AUTHORIZATION_WINDOW_H
#define CLIENT_ANDROID_AUTHORIZATION_WINDOW_H

#include <QList>
#include <QWidget>

#include "client/config.h"

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class AppBar;
class ComboBox;
class Label;
class LineEdit;
class QWidget;
class RadioButton;
class Switch;

// Asks for the credentials of a host before its session is opened. A screen rather than a dialog:
// it stays usable with the on-screen keyboard up.
class AuthorizationWindow final : public QWidget
{
    Q_OBJECT

public:
    // |save_credentials_available| enables the switch that keeps the credentials on the device.
    AuthorizationWindow(const HostConfig& host, proto::peer::SessionType session_type,
                        bool save_credentials_available,
                        const QList<CredentialConfig>& credentials, QWidget* parent = nullptr);
    ~AuthorizationWindow() final;

    // The host with the entered credentials. The user name is empty when a one-time password is
    // used; the caller then connects by host id.
    HostConfig host() const;
    proto::peer::SessionType sessionType() const { return session_type_; }

    // The record the user chose, 0 when the user name and the password were typed here.
    qint64 credentialId() const;

    bool isSaveCredentialsChecked() const;

signals:
    void sig_accepted();
    void sig_closed();

private slots:
    void onConnectClicked();
    void onModeToggled(bool checked);

    // Lifts the content by however much the on-screen keyboard overlaps the window. Android's
    // adjustResize is unreliable here (it sometimes leaves the window full height with the keyboard
    // up), so the keyboard rectangle reported by Qt is the source of truth.
    void updateKeyboardInset();

private:
    void showError(const QString& message);

    bool usesOneTimePassword() const;
    bool isOneTimePasswordOffered() const;
    bool usesSavedCredentials() const;
    bool hasSavedCredentials() const;
    const CredentialConfig* selectedCredential() const;
    void updateModes();

    const HostConfig host_;
    const proto::peer::SessionType session_type_;
    const QList<CredentialConfig> credentials_;

    AppBar* app_bar_ = nullptr;
    RadioButton* radio_user_password_ = nullptr;
    RadioButton* radio_one_time_password_ = nullptr;
    RadioButton* radio_saved_credentials_ = nullptr;
    QWidget* user_password_block_ = nullptr;
    QWidget* one_time_password_block_ = nullptr;
    QWidget* saved_credentials_block_ = nullptr;
    LineEdit* edit_username_ = nullptr;
    LineEdit* edit_password_ = nullptr;
    LineEdit* edit_one_time_password_ = nullptr;
    ComboBox* combo_credential_ = nullptr;
    Switch* switch_save_credentials_ = nullptr;
    Label* label_error_ = nullptr;

    Q_DISABLE_COPY_MOVE(AuthorizationWindow)
};

#endif // CLIENT_ANDROID_AUTHORIZATION_WINDOW_H
