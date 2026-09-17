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

#ifndef CLIENT_ANDROID_CREDENTIAL_EDITOR_H
#define CLIENT_ANDROID_CREDENTIAL_EDITOR_H

#include <QWidget>

class Button;
class Label;
class LineEdit;

class CredentialEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit CredentialEditor(QWidget* parent = nullptr);
    ~CredentialEditor() final;

    // Resets the form for adding a new record.
    void prepareForAdd();

    // Loads the record |credential_id| into the form for editing. Returns false if the record does
    // not exist; the form keeps its previous state and must not be shown.
    bool prepareForEdit(qint64 credential_id);

signals:
    // Emitted after the record has been saved to or removed from the database.
    void sig_accepted();

private slots:
    void onSaveClicked();
    void onDeleteClicked();

private:
    void showError(const QString& message);

    LineEdit* name_ = nullptr;
    LineEdit* username_ = nullptr;
    LineEdit* password_ = nullptr;
    Label* error_ = nullptr;
    Button* delete_button_ = nullptr;
    qint64 credential_id_ = -1;

    Q_DISABLE_COPY_MOVE(CredentialEditor)
};

#endif // CLIENT_ANDROID_CREDENTIAL_EDITOR_H
