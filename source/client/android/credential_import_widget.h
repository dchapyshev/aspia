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

#ifndef CLIENT_ANDROID_CREDENTIAL_IMPORT_WIDGET_H
#define CLIENT_ANDROID_CREDENTIAL_IMPORT_WIDGET_H

#include <QList>
#include <QWidget>

#include "client/config.h"

class Button;
class CredentialSelectionList;
class IconButton;
class Label;
class LineEdit;

class CredentialImportWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit CredentialImportWidget(QWidget* parent = nullptr);
    ~CredentialImportWidget() final;

    QList<QWidget*> appBarActions() const;

    // Resets the form for the file at |file_path|, which is opened once the password is entered.
    void prepare(const QString& file_path);

    // Forgets the records and the password, so that nothing decrypted outlives the screen.
    void clear();

signals:
    // Emitted after the checked records have been written to the database.
    void sig_finished();

private slots:
    void onOpenClicked();
    void onImportClicked();

private:
    void setOpened(bool opened);
    void updateImportButton();
    void showError(const QString& message);

    CredentialSelectionList* list_ = nullptr;
    Label* label_description_ = nullptr;
    LineEdit* edit_password_ = nullptr;
    Label* label_error_ = nullptr;
    Button* button_open_ = nullptr;
    Button* button_import_ = nullptr;
    IconButton* button_select_all_ = nullptr;

    QString file_path_;
    QList<CredentialConfig> credentials_;

    Q_DISABLE_COPY_MOVE(CredentialImportWidget)
};

#endif // CLIENT_ANDROID_CREDENTIAL_IMPORT_WIDGET_H
