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

#ifndef CLIENT_ANDROID_LOCAL_HOST_EDITOR_H
#define CLIENT_ANDROID_LOCAL_HOST_EDITOR_H

#include <QWidget>

class Button;
class ComboBox;
class Label;
class LineEdit;
class TextArea;

// Full-screen editor for a local host. The router selector switches the address field between a
// direct address and a host ID; on save the host is written to the local database.
class LocalHostEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit LocalHostEditor(QWidget* parent = nullptr);
    ~LocalHostEditor() final;

    // Resets the form for adding a new host to |group_id| and reloads the router list.
    void prepareForAdd(qint64 group_id);

    // Loads the host |host_id| into the form for editing. Returns false if the host does not
    // exist; the form keeps its previous state and must not be shown.
    bool prepareForEdit(qint64 host_id);

signals:
    // Emitted after the host has been saved to or removed from the database.
    void sig_accepted();

private slots:
    void onRouterChanged();
    void onSaveClicked();
    void onDeleteClicked();

private:
    void loadRouters(qint64 selected_router_id);
    void showError(const QString& message);

    ComboBox* router_ = nullptr;
    LineEdit* name_ = nullptr;
    LineEdit* address_ = nullptr;
    LineEdit* username_ = nullptr;
    LineEdit* password_ = nullptr;
    TextArea* comment_ = nullptr;
    Label* error_ = nullptr;
    Button* delete_button_ = nullptr;
    qint64 entry_id_ = -1;
    qint64 group_id_ = 0;

    Q_DISABLE_COPY_MOVE(LocalHostEditor)
};

#endif // CLIENT_ANDROID_LOCAL_HOST_EDITOR_H
