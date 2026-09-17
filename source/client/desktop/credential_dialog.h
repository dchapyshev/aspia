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

#ifndef CLIENT_DESKTOP_CREDENTIAL_DIALOG_H
#define CLIENT_DESKTOP_CREDENTIAL_DIALOG_H

#include <QDialog>

#include <memory>

class QAbstractButton;

namespace Ui {
class CredentialDialog;
} // namespace Ui

class CredentialDialog final : public QDialog
{
    Q_OBJECT

public:
    // |credential_id| of -1 adds a record.
    CredentialDialog(qint64 credential_id, QWidget* parent = nullptr);
    ~CredentialDialog() final;

    // The record written, once the dialog is accepted.
    qint64 credentialId() const { return credential_id_; }

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    std::unique_ptr<Ui::CredentialDialog> ui;
    qint64 credential_id_ = -1;

    Q_DISABLE_COPY_MOVE(CredentialDialog)
};

#endif // CLIENT_DESKTOP_CREDENTIAL_DIALOG_H
