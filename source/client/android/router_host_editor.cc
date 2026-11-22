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

#include "client/android/router_host_editor.h"

#include <QVBoxLayout>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/android/credential_combo_box.h"
#include "client/android/profile_combo_box.h"
#include "client/config.h"
#include "client/router/router_controller.h"
#include "client/storage/router_host_store.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterHostEditor::RouterHostEditor(QWidget* parent)
    : QWidget(parent),
      switch_saved_credentials_(new Switch(tr("Use existing"))),
      edit_username_(new LineEdit()),
      edit_password_(new LineEdit()),
      combo_credential_(new CredentialComboBox()),
      combo_profile_(new ProfileComboBox()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    edit_username_->setLabel(tr("User Name"));

    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

    combo_credential_->setLabel(tr("Credentials"));
    combo_profile_->setLabel(tr("Profile"));

    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(combo_profile_);
    form_layout->addWidget(switch_saved_credentials_);
    form_layout->addWidget(edit_username_);
    form_layout->addWidget(edit_password_);
    form_layout->addWidget(combo_credential_);
    form_layout->addWidget(save);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    combo_credential_->setUnreadableShown(true);
    connect(combo_credential_, &CredentialComboBox::sig_reloaded, this, [this]()
    {
        if (combo_credential_->currentIndex() < 0)
        {
            switch_saved_credentials_->setChecked(false);
            combo_credential_->setCurrentIndex(0);
        }

        switch_saved_credentials_->setEnabled(combo_credential_->count() > 0);
    });

    connect(switch_saved_credentials_, &Switch::toggled, this, &RouterHostEditor::onSavedCredentialsToggled);
    connect(save, &Button::clicked, this, &RouterHostEditor::onSaveClicked);

    connect(switch_saved_credentials_, &Switch::clicked, this, [this]() { credentials_edited_ = true; });
    connect(combo_credential_, &CredentialComboBox::activated, this, [this]() { credentials_edited_ = true; });
    connect(edit_username_, &LineEdit::textEdited, this, [this]() { credentials_edited_ = true; });
    connect(edit_password_, &LineEdit::textEdited, this, [this]() { credentials_edited_ = true; });
    connect(combo_profile_, &ProfileComboBox::activated, this, [this]() { profile_edited_ = true; });
}

//--------------------------------------------------------------------------------------------------
RouterHostEditor::~RouterHostEditor() = default;

//--------------------------------------------------------------------------------------------------
bool RouterHostEditor::prepareForEdit(HostId host_id)
{
    // A temporary host id is handed out at random and comes back for another machine, so what was
    // saved under it would be sent to a host the user never gave it to.
    if (host_id == kInvalidHostId || isTempHostId(host_id))
    {
        LOG(ERROR) << "Credentials cannot be kept for host" << host_id;
        return false;
    }

    host_id_ = host_id;
    credentials_edited_ = false;
    profile_edited_ = false;

    edit_username_->clear();
    edit_password_->clear();
    label_error_->setVisible(false);

    qint64 credential_id = 0;
    qint64 profile_id = 0;

    RouterHostConfig credentials;
    const Database::FindResult found =
        RouterHostStore().find(host_id_, &credentials);

    if (found == Database::FindResult::FOUND || found == Database::FindResult::UNREADABLE)
    {
        edit_username_->setText(credentials.username());
        edit_password_->setText(credentials.password().toString());
        credential_id = credentials.credentialId();
        profile_id = credentials.profileId();
    }

    const bool credentials_list_loaded = loadCredentials(credential_id);
    const bool profiles_loaded = loadProfiles(profile_id);
    credentials_loaded_ =
        credentials_list_loaded && profiles_loaded && found != Database::FindResult::FAILED;
    edit_username_->setFocus();

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the credentials of host" << host_id_;
        showError(tr("Failed to read data from the local database."));
    }
    else if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Credentials of host" << host_id_ << "could not be read";
        if (credentials_loaded_)
            showError(tr("The credentials of the host are damaged. Enter them again."));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::onSavedCredentialsToggled(bool checked)
{
    if (checked && edit_username_->text().isEmpty() != edit_password_->text().isEmpty())
    {
        edit_username_->clear();
        edit_password_->clear();
    }

    edit_username_->setVisible(!checked);
    edit_password_->setVisible(!checked);
    combo_credential_->setVisible(checked);
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::onSaveClicked()
{
    if (!credentials_loaded_)
    {
        showError(tr("Failed to read data from the local database."));
        return;
    }

    const qint64 credential_id =
        switch_saved_credentials_->isChecked() ? combo_credential_->currentData().toLongLong() : 0;
    const QString username = edit_username_->text();
    const QString password = edit_password_->text();

    if (username.isEmpty() != password.isEmpty())
    {
        showError(tr("Enter both the user name and the password, or leave both empty."));
        return;
    }

    RouterHostStore store;

    RouterHostConfig stored;
    const Database::FindResult found = store.find(host_id_, &stored);
    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the credentials of host" << host_id_;
        showError(tr("Failed to save the credentials."));
        return;
    }

    if (credentials_edited_ && (!username.isEmpty() || credential_id > 0 || found == Database::FindResult::FOUND))
    {
        RouterHostConfig credentials;
        credentials.setHostId(host_id_);
        credentials.setCredentialId(credential_id);
        credentials.setUsername(username);
        credentials.setPassword(SecureString(password));

        bool saved = false;

        if (found == Database::FindResult::NOT_FOUND)
            saved = store.add(credentials);
        else
            saved = store.modify(credentials);

        if (!saved)
        {
            showError(tr("Failed to save the credentials."));
            return;
        }
    }

    if (profile_edited_ && !store.setProfileId(host_id_, combo_profile_->currentProfileId()))
    {
        showError(tr("Failed to save the profile."));
        return;
    }

    RouterController::startSyncRecordExchange();
    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
bool RouterHostEditor::loadCredentials(qint64 selected_credential_id)
{
    if (!combo_credential_->onReload())
    {
        showError(tr("Failed to read data from the local database."));
        return false;
    }

    combo_credential_->setCurrentCredentialId(selected_credential_id);

    const bool selected = combo_credential_->currentIndex() >= 0;
    if (!selected)
        combo_credential_->setCurrentIndex(0);

    // Nothing to share until a record of credentials is added.
    switch_saved_credentials_->setEnabled(combo_credential_->count() > 0);
    switch_saved_credentials_->setChecked(selected);
    onSavedCredentialsToggled(switch_saved_credentials_->isChecked());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostEditor::loadProfiles(qint64 selected_profile_id)
{
    if (!combo_profile_->onReload())
    {
        showError(tr("Failed to read data from the local database."));
        return false;
    }

    combo_profile_->setCurrentProfileId(selected_profile_id);
    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
