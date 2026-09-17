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

#include "client/android/authorization_window.h"

#include <QGuiApplication>
#include <QInputMethod>
#include <QButtonGroup>
#include <QVBoxLayout>

#include "common/android/app_bar.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/combo_box.h"
#include "common/android/radio_button.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"
#include "proto/peer.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

constexpr int kChoiceIndent = 24;

} // namespace

//--------------------------------------------------------------------------------------------------
AuthorizationWindow::AuthorizationWindow(const HostConfig& host, proto::peer::SessionType session_type,
                                         bool save_credentials_available,
                                         const QList<CredentialConfig>& credentials, QWidget* parent)
    : QWidget(parent),
      host_(host),
      session_type_(session_type),
      credentials_(credentials),
      app_bar_(new AppBar(this)),
      radio_user_password_(new RadioButton(tr("Enter user name and password"))),
      radio_one_time_password_(new RadioButton(tr("One-time password connection"))),
      radio_saved_credentials_(new RadioButton(tr("Use saved credentials"))),
      user_password_block_(new QWidget()),
      one_time_password_block_(new QWidget()),
      saved_credentials_block_(new QWidget()),
      edit_username_(new LineEdit()),
      edit_password_(new LineEdit()),
      edit_one_time_password_(new LineEdit()),
      combo_credential_(new ComboBox()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    app_bar_->setTitle(tr("Authorization"));
    app_bar_->setBackVisible(true);
    connect(app_bar_, &AppBar::sig_backClicked, this, &AuthorizationWindow::sig_closed);

    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    edit_username_->setLabel(tr("User Name"));
    edit_username_->setText(host.username());

    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

    edit_one_time_password_->setLabel(tr("Password"));
    edit_one_time_password_->setEchoMode(QLineEdit::Password);

    combo_credential_->setLabel(tr("Credentials"));
    for (const CredentialConfig& credential : credentials_)
        combo_credential_->addItem(credential.displayName(), QVariant::fromValue(credential.id()));

    Button* connect_button = new Button(tr("Connect"), Button::Role::FILLED);

    QVBoxLayout* user_password_layout = new QVBoxLayout(user_password_block_);
    user_password_layout->setContentsMargins(0, 0, 0, 0);
    user_password_layout->setSpacing(kFormSpacing);
    user_password_layout->addWidget(edit_username_);
    user_password_layout->addWidget(edit_password_);

    QVBoxLayout* one_time_password_layout = new QVBoxLayout(one_time_password_block_);
    one_time_password_layout->setContentsMargins(0, 0, 0, 0);
    one_time_password_layout->setSpacing(kFormSpacing);
    one_time_password_layout->addWidget(edit_one_time_password_);

    QVBoxLayout* saved_credentials_layout = new QVBoxLayout(saved_credentials_block_);
    saved_credentials_layout->setContentsMargins(0, 0, 0, 0);
    saved_credentials_layout->setSpacing(kFormSpacing);
    saved_credentials_layout->addWidget(combo_credential_);

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(radio_user_password_);
    form_layout->addWidget(user_password_block_);
    form_layout->addWidget(radio_one_time_password_);
    form_layout->addWidget(one_time_password_block_);
    form_layout->addWidget(radio_saved_credentials_);
    form_layout->addWidget(saved_credentials_block_);

    if (save_credentials_available)
    {
        switch_save_credentials_ = new Switch(tr("Save sign-in data"));
        switch_save_credentials_->setChecked(true);
        form_layout->addWidget(switch_save_credentials_);
    }

    form_layout->addStretch();
    form_layout->addWidget(connect_button);

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(scroll, 1);

    // Qt keeps radio buttons exclusive between siblings, and these are not siblings.
    QButtonGroup* modes = new QButtonGroup(this);

    for (RadioButton* button : { radio_user_password_, radio_one_time_password_, radio_saved_credentials_ })
    {
        modes->addButton(button);
        connect(button, &RadioButton::toggled, this, &AuthorizationWindow::onModeToggled);
    }

    if (isOneTimePasswordOffered() && host.username().isEmpty())
        radio_one_time_password_->setChecked(true);
    else
        radio_user_password_->setChecked(true);

    updateModes();

    connect(connect_button, &Button::clicked, this, &AuthorizationWindow::onConnectClicked);
    connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged,
            this, &AuthorizationWindow::updateKeyboardInset);
}

//--------------------------------------------------------------------------------------------------
AuthorizationWindow::~AuthorizationWindow() = default;

//--------------------------------------------------------------------------------------------------
HostConfig AuthorizationWindow::host() const
{
    HostConfig host = host_;

    const CredentialConfig* credential = selectedCredential();
    if (credential)
    {
        host.setUsername(credential->username());
        host.setPassword(credential->password());
        return host;
    }

    if (usesOneTimePassword())
    {
        host.setUsername(QString());
        host.setPassword(SecureString(edit_one_time_password_->text()));
        return host;
    }

    host.setUsername(edit_username_->text());
    host.setPassword(SecureString(edit_password_->text()));
    return host;
}

//--------------------------------------------------------------------------------------------------
qint64 AuthorizationWindow::credentialId() const
{
    const CredentialConfig* credential = selectedCredential();
    return credential ? credential->id() : 0;
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationWindow::isSaveCredentialsChecked() const
{
    // The switch is hidden while a one-time password is asked for, and what the user was never
    // shown is not his answer.
    return switch_save_credentials_ && !usesOneTimePassword() && switch_save_credentials_->isChecked();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationWindow::onConnectClicked()
{
    if (usesOneTimePassword())
    {
        if (edit_one_time_password_->text().isEmpty())
        {
            showError(tr("Password cannot be empty."));
            edit_one_time_password_->setFocus();
            return;
        }
    }
    else if (!usesSavedCredentials())
    {
        if (edit_username_->text().isEmpty())
        {
            showError(tr("User name cannot be empty."));
            edit_username_->setFocus();
            return;
        }

        if (edit_password_->text().isEmpty())
        {
            showError(tr("Password cannot be empty."));
            edit_password_->setFocus();
            return;
        }
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationWindow::onModeToggled(bool /* checked */)
{
    updateModes();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationWindow::usesOneTimePassword() const
{
    return isOneTimePasswordOffered() && radio_one_time_password_->isChecked();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationWindow::isOneTimePasswordOffered() const
{
    return host_.routerId() > 0;
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationWindow::hasSavedCredentials() const
{
    return !credentials_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationWindow::usesSavedCredentials() const
{
    return hasSavedCredentials() && radio_saved_credentials_->isChecked();
}

//--------------------------------------------------------------------------------------------------
const CredentialConfig* AuthorizationWindow::selectedCredential() const
{
    if (!usesSavedCredentials())
        return nullptr;

    const qint64 credential_id = combo_credential_->currentData().toLongLong();
    for (const CredentialConfig& credential : credentials_)
    {
        if (credential.id() == credential_id)
            return &credential;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void AuthorizationWindow::updateModes()
{
    const bool one_time_offered = isOneTimePasswordOffered();
    const bool saved_credentials_offered = hasSavedCredentials();

    // The indent is what makes a block belong to the choice above it.
    const bool has_choice = one_time_offered || saved_credentials_offered;
    const int indent = has_choice ? kChoiceIndent : 0;

    for (QWidget* block : { user_password_block_, one_time_password_block_, saved_credentials_block_ })
        block->layout()->setContentsMargins(indent, 0, 0, 0);

    radio_user_password_->setVisible(has_choice);
    radio_one_time_password_->setVisible(one_time_offered);
    radio_saved_credentials_->setVisible(saved_credentials_offered);

    one_time_password_block_->setVisible(one_time_offered);
    saved_credentials_block_->setVisible(saved_credentials_offered);

    const bool one_time = usesOneTimePassword();
    const bool saved = usesSavedCredentials();

    user_password_block_->setEnabled(!one_time && !saved);
    one_time_password_block_->setEnabled(one_time);
    saved_credentials_block_->setEnabled(saved);

    // A one-time password is good for one connection, so there is nothing to keep.
    if (switch_save_credentials_)
        switch_save_credentials_->setVisible(!one_time);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationWindow::updateKeyboardInset()
{
    const QInputMethod* input_method = QGuiApplication::inputMethod();

    int inset = 0;
    if (input_method->isVisible())
    {
        // How far the window extends past the top of the keyboard (0 when Android already resized
        // the window to sit above it). The window rect is in logical pixels while the keyboard
        // rectangle is in physical pixels, so scale the latter down.
        const QRect window_rect(mapToGlobal(QPoint(0, 0)), size());
        const int keyboard_top = qRound(input_method->keyboardRectangle().top() / devicePixelRatioF());
        inset = qMax(0, window_rect.bottom() - keyboard_top);
    }

    layout()->setContentsMargins(0, 0, 0, inset);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationWindow::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
