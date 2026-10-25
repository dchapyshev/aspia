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

#ifndef CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_PANEL_H
#define CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_PANEL_H

#include <QList>
#include <QWidget>

#include "client/config.h"

class QComboBox;
class QLabel;
class QToolButton;

class CredentialPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit CredentialPanel(QWidget* parent);
    ~CredentialPanel() final;

    void popup(qint64 credential_id, const QString& message = QString());
    bool signIn();

public slots:
    void onDismiss();

signals:
    void sig_credentialSelected(qint64 credential_id, const QString& user_name, const SecureString& password);

protected:
    // QObject implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;

private slots:
    void onSignIn();
    void onCurrentCredentialChanged();

private:
    void updatePosition();

    QLabel* label_message_ = nullptr;
    QComboBox* combo_credential_ = nullptr;
    QToolButton* sign_in_button_ = nullptr;
    QList<CredentialConfig> credentials_;

    Q_DISABLE_COPY_MOVE(CredentialPanel)
};

#endif // CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_PANEL_H
