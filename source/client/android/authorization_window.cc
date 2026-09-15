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
#include <QVBoxLayout>

#include "common/android/app_bar.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"
#include "proto/peer.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
AuthorizationWindow::AuthorizationWindow(const HostConfig& host, proto::peer::SessionType session_type,
                                         bool save_credentials_available, QWidget* parent)
    : QWidget(parent),
      host_(host),
      session_type_(session_type),
      app_bar_(new AppBar(this)),
      username_(new LineEdit()),
      password_(new LineEdit()),
      error_(new Label(QString(), Label::Role::CAPTION))
{
    app_bar_->setTitle(tr("Authorization"));
    app_bar_->setBackVisible(true);
    connect(app_bar_, &AppBar::sig_backClicked, this, &AuthorizationWindow::sig_closed);

    Label* text = new Label(tr("Enter the credentials to connect to the host."), Label::Role::BODY);
    text->setWordWrap(true);

    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    username_->setLabel(tr("Username"));
    username_->setText(host.username());

    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    Button* connect_button = new Button(tr("Connect"), Button::Role::FILLED);

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(text);
    form_layout->addWidget(error_);
    form_layout->addWidget(username_);
    form_layout->addWidget(password_);

    // Router hosts also accept a one-time password. A saved user name means a named-user
    // connection, so the switch starts off then.
    if (host.routerId() > 0)
    {
        one_time_password_ = new Switch(tr("One-time password"));
        one_time_password_->setChecked(host.username().isEmpty());
        connect(one_time_password_, &Switch::toggled,
                this, &AuthorizationWindow::onOneTimePasswordToggled);
        form_layout->addWidget(one_time_password_);
    }

    if (save_credentials_available)
    {
        save_credentials_ = new Switch(tr("Save credentials"));
        save_credentials_->setChecked(true);
        form_layout->addWidget(save_credentials_);
    }

    form_layout->addWidget(connect_button);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(scroll, 1);

    onOneTimePasswordToggled(one_time_password_ && one_time_password_->isChecked());

    connect(connect_button, &Button::clicked, this, &AuthorizationWindow::onConnectClicked);
    connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged,
            this, &AuthorizationWindow::updateKeyboardInset);
}

//--------------------------------------------------------------------------------------------------
AuthorizationWindow::~AuthorizationWindow() = default;

//--------------------------------------------------------------------------------------------------
HostConfig AuthorizationWindow::host() const
{
    const bool one_time = one_time_password_ && one_time_password_->isChecked();

    HostConfig host = host_;
    host.setUsername(one_time ? QString() : username_->text());
    host.setPassword(SecureString(password_->text()));
    return host;
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationWindow::isSaveCredentialsChecked() const
{
    // The switch is hidden while a one-time password is asked for, and what the user was never
    // shown is not his answer.
    const bool one_time = one_time_password_ && one_time_password_->isChecked();
    return save_credentials_ && !one_time && save_credentials_->isChecked();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationWindow::onConnectClicked()
{
    const bool one_time = one_time_password_ && one_time_password_->isChecked();

    if (!one_time && username_->text().isEmpty())
    {
        showError(tr("Username cannot be empty."));
        username_->setFocus();
        return;
    }

    if (password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        password_->setFocus();
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationWindow::onOneTimePasswordToggled(bool checked)
{
    username_->setVisible(!checked);
    if (checked)
        username_->clear();

    if (save_credentials_)
        save_credentials_->setVisible(!checked);
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
    error_->setText(message);
    error_->setVisible(true);
}
