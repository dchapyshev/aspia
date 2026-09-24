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

#ifndef CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_EXPORT_DIALOG_H
#define CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_EXPORT_DIALOG_H

#include <QDialog>

#include <memory>

#include "client/config.h"

class QAbstractButton;

namespace Ui {
class CredentialExportDialog;
} // namespace Ui

class CredentialExportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CredentialExportDialog(QWidget* parent = nullptr);
    ~CredentialExportDialog() final;

private slots:
    void onCheckAllButtonPressed();
    void onCheckNoneButtonPressed();
    void onButtonBoxClicked(QAbstractButton* button);
    void onLoadData();

private:
    void setCheckState(Qt::CheckState state);
    QList<CredentialConfig> checkedCredentials() const;
    bool checkPassword();

    std::unique_ptr<Ui::CredentialExportDialog> ui;
    QList<CredentialConfig> credentials_;

    Q_DISABLE_COPY_MOVE(CredentialExportDialog)
};

#endif // CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_EXPORT_DIALOG_H
