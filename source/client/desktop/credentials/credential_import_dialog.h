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

#ifndef CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_IMPORT_DIALOG_H
#define CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_IMPORT_DIALOG_H

#include <QDialog>

#include <memory>

#include "client/config.h"

class QAbstractButton;
class QPushButton;

namespace Ui {
class CredentialImportDialog;
} // namespace Ui

class CredentialImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CredentialImportDialog(QWidget* parent = nullptr);
    ~CredentialImportDialog() final;

private slots:
    void onBrowseButtonPressed();
    void onOpenButtonPressed();
    void onCheckAllButtonPressed();
    void onCheckNoneButtonPressed();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void clearCredentials();
    void setCheckState(Qt::CheckState state);
    void updateButtons();
    QList<CredentialConfig> checkedCredentials() const;

    std::unique_ptr<Ui::CredentialImportDialog> ui;
    QPushButton* import_button_ = nullptr;
    QList<CredentialConfig> credentials_;

    Q_DISABLE_COPY_MOVE(CredentialImportDialog)
};

#endif // CLIENT_DESKTOP_CREDENTIALS_CREDENTIAL_IMPORT_DIALOG_H
