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

#ifndef CLIENT_ANDROID_PROFILE_EDITOR_H
#define CLIENT_ANDROID_PROFILE_EDITOR_H

#include <QWidget>

#include "proto/desktop_control.h"

class Button;
class ComboBox;
class Label;
class LineEdit;
class Switch;

class ProfileEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileEditor(QWidget* parent = nullptr);
    ~ProfileEditor() final;

    // Resets the form for adding a new profile with the parameters of the default one.
    void prepareForAdd();

    // Loads the profile |profile_id| into the form for editing, 0 is the default profile. Returns
    // false if the profile does not exist; the form keeps its previous state and must not be shown.
    bool prepareForEdit(qint64 profile_id);

signals:
    // Emitted after the profile has been saved to or removed from the database.
    void sig_accepted();

private slots:
    void onProfilesChanged();
    void onSaveClicked();
    void onDeleteClicked();

private:
    void showParameters(const proto::control::Config& desktop_config, quint32 udp_methods);
    void showError(const QString& message);

    LineEdit* edit_name_ = nullptr;
    Switch* switch_audio_ = nullptr;
    Switch* switch_clipboard_ = nullptr;
    Switch* switch_cursor_shape_ = nullptr;
    Switch* switch_cursor_position_ = nullptr;
    Switch* switch_effects_ = nullptr;
    Switch* switch_wallpaper_ = nullptr;
    Switch* switch_lock_at_disconnect_ = nullptr;
    Switch* switch_block_input_ = nullptr;
    ComboBox* combo_quality_ = nullptr;
    Switch* switch_udp_direct_ = nullptr;
    Switch* switch_udp_hole_punching_ = nullptr;
    Switch* switch_udp_pcp_ = nullptr;
    Switch* switch_udp_natpmp_ = nullptr;
    Switch* switch_udp_upnp_ = nullptr;
    Label* label_error_ = nullptr;
    Button* button_delete_ = nullptr;

    qint64 profile_id_ = -1;
    qint64 modify_time_ = 0;
    proto::control::Config desktop_config_;

    Q_DISABLE_COPY_MOVE(ProfileEditor)
};

#endif // CLIENT_ANDROID_PROFILE_EDITOR_H
