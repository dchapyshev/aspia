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

#ifndef CLIENT_ANDROID_CREDENTIAL_EXPORT_WIDGET_H
#define CLIENT_ANDROID_CREDENTIAL_EXPORT_WIDGET_H

#include <QList>
#include <QWidget>

#include "client/config.h"

class Button;
class CredentialSelectionList;
class IconButton;
class Label;
class LineEdit;

class CredentialExportWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit CredentialExportWidget(QWidget* parent = nullptr);
    ~CredentialExportWidget() final;

    QList<QWidget*> appBarActions() const;

    // Reads the records from the database and resets the form.
    void prepare();

    // Forgets the records and the password, so that nothing decrypted outlives the screen.
    void clear();

signals:
    // Emitted after the file has been written.
    void sig_finished();

private slots:
    void onSaveClicked();

private:
    void updateSaveButton();
    QList<CredentialConfig> checkedCredentials() const;
    bool checkPassword();
    void showError(const QString& message);

    CredentialSelectionList* list_ = nullptr;
    LineEdit* edit_password_ = nullptr;
    Label* label_error_ = nullptr;
    Button* button_save_ = nullptr;
    IconButton* button_select_all_ = nullptr;

    QList<CredentialConfig> credentials_;

    Q_DISABLE_COPY_MOVE(CredentialExportWidget)
};

#endif // CLIENT_ANDROID_CREDENTIAL_EXPORT_WIDGET_H
