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

#include "client/android/credential_combo_box.h"

#include "base/gui_application.h"
#include "base/logging.h"
#include "client/router/router_controller.h"
#include "client/storage/database.h"

//--------------------------------------------------------------------------------------------------
CredentialComboBox::CredentialComboBox(QWidget* parent)
    : ComboBox(parent)
{
    connect(&RouterController::instance(), &RouterController::sig_credentialsChanged,
            this, &CredentialComboBox::onReload);
}

//--------------------------------------------------------------------------------------------------
CredentialComboBox::~CredentialComboBox() = default;

//--------------------------------------------------------------------------------------------------
void CredentialComboBox::setCredentialType(CredentialConfig::Type type)
{
    type_ = type;
}

//--------------------------------------------------------------------------------------------------
void CredentialComboBox::setUnreadableShown(bool shown)
{
    unreadable_shown_ = shown;
}

//--------------------------------------------------------------------------------------------------
qint64 CredentialComboBox::currentCredentialId() const
{
    return currentIndex() >= 0 ? currentData().toLongLong() : 0;
}

//--------------------------------------------------------------------------------------------------
void CredentialComboBox::setCurrentCredentialId(qint64 credential_id)
{
    setCurrentIndex(findData(QVariant::fromValue(credential_id)));
}

//--------------------------------------------------------------------------------------------------
const CredentialConfig* CredentialComboBox::currentCredential() const
{
    const qint64 credential_id = currentCredentialId();

    for (const CredentialConfig& credential : std::as_const(credentials_))
    {
        if (credential.id() == credential_id)
            return &credential;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
bool CredentialComboBox::onReload()
{
    QList<CredentialConfig> credentials;
    if (Database::instance().credentialList(&credentials, type_) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of credentials";
        return false;
    }

    const qint64 credential_id = currentCredentialId();

    const QIcon icon = GuiApplication::svgIcon(
        type_ == CredentialConfig::Type::OS ? ":/img/key-os.svg" : ":/img/keys.svg");
    const QIcon unread_icon = GuiApplication::svgIcon(":/img/key-corrupted.svg");

    clear();
    credentials_.clear();

    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        if (!credential.isValid() && !unreadable_shown_)
            continue;

        credentials_.append(credential);
        addItem(credential.isValid() ? icon : unread_icon, credential.displayName(),
                QVariant::fromValue(credential.id()));
    }

    setCurrentCredentialId(credential_id);

    emit sig_reloaded();
    return true;
}
