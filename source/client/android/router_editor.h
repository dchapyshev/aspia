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

#ifndef CLIENT_ANDROID_ROUTER_EDITOR_H
#define CLIENT_ANDROID_ROUTER_EDITOR_H

#include <QWidget>

class Button;
class Label;
class LineEdit;
class Switch;

// Full-screen editor for a router. On save the router is written to the local database.
class RouterEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit RouterEditor(QWidget* parent = nullptr);
    ~RouterEditor() final;

    // Resets the form for adding a new router.
    void prepareForAdd();

    // Loads the router into the form for editing. Returns false if the router is not set; the form
    // keeps its previous state and must not be shown.
    bool prepareForEdit();

signals:
    // Emitted after the router has been saved to the database.
    void sig_accepted();

private slots:
    void onSaveClicked();
    void onResetSyncClicked();
    void onRouterStatusChanged();

private:
    void showError(const QString& message);

    LineEdit* edit_address_ = nullptr;
    LineEdit* edit_username_ = nullptr;
    LineEdit* edit_password_ = nullptr;
    Switch* switch_enabled_ = nullptr;
    Switch* switch_sync_ = nullptr;
    Label* label_error_ = nullptr;
    Button* button_reset_sync_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterEditor)
};

#endif // CLIENT_ANDROID_ROUTER_EDITOR_H
