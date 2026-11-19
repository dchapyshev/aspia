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

#ifndef CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_COMBO_BOX_H
#define CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_COMBO_BOX_H

#include <QComboBox>
#include <QList>

#include "client/config.h"

class CredentialComboBox final : public QComboBox
{
    Q_OBJECT

public:
    explicit CredentialComboBox(QWidget* parent = nullptr);
    ~CredentialComboBox() final;

    void setCredentialType(CredentialConfig::Type type);
    void setUnreadableShown(bool shown);

    qint64 currentCredentialId() const;
    void setCurrentCredentialId(qint64 credential_id);
    const CredentialConfig* currentCredential() const;

    void clearCredentials();

public slots:
    bool onReload();

signals:
    void sig_reloaded();

private slots:
    void onCredentialsChanged();

private:
    CredentialConfig::Type type_ = CredentialConfig::Type::ASPIA;
    bool unreadable_shown_ = false;
    bool loaded_ = false;
    QList<CredentialConfig> credentials_;

    Q_DISABLE_COPY_MOVE(CredentialComboBox)
};

#endif // CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_COMBO_BOX_H
