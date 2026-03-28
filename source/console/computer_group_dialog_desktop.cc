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

#include "console/computer_group_dialog_desktop.h"

#include "base/logging.h"

namespace console {

//--------------------------------------------------------------------------------------------------
ComputerGroupDialogDesktop::ComputerGroupDialogDesktop(int type, bool is_root_group, QWidget* parent)
    : ComputerGroupDialogTab(type, is_root_group, parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    connect(ui.combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComputerGroupDialogDesktop::onCodecChanged);

    if (is_root_group)
    {
        ui.checkbox_inherit_config->setVisible(false);
    }
    else
    {
        ui.checkbox_inherit_config->setVisible(true);

        connect(ui.checkbox_inherit_config, &QCheckBox::toggled, this, [this](bool checked)
        {
            ui.groupbox_codec->setEnabled(!checked);
            ui.groupbox_features->setEnabled(!checked);
            ui.groupbox_appearance->setEnabled(!checked);
            ui.groupbox_other->setEnabled(!checked);
        });
    }
}

//--------------------------------------------------------------------------------------------------
ComputerGroupDialogDesktop::~ComputerGroupDialogDesktop()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogDesktop::restoreSettings(
    proto::peer::SessionType session_type, const proto::address_book::ComputerGroupConfig& group_config)
{
    proto::control::Config desktop_config;

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        ui.checkbox_inherit_config->setChecked(group_config.inherit().desktop_manage());
        desktop_config = group_config.session_config().desktop_manage();
    }
    else
    {
        DCHECK_EQ(session_type, proto::peer::SESSION_TYPE_DESKTOP_VIEW);

        ui.checkbox_inherit_config->setChecked(group_config.inherit().desktop_view());
        desktop_config = group_config.session_config().desktop_view();
    }

    QComboBox* combo_codec = ui.combo_codec;
    combo_codec->addItem("VP9", proto::video::ENCODING_VP9);
    combo_codec->addItem("VP8", proto::video::ENCODING_VP8);

    int current_codec = combo_codec->findData(desktop_config.video_encoding());
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);
    onCodecChanged(current_codec);

    if (desktop_config.audio_encoding() != proto::audio::ENCODING_UNKNOWN)
        ui.checkbox_audio->setChecked(true);

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (desktop_config.flags() & proto::control::LOCK_AT_DISCONNECT)
            ui.checkbox_lock_at_disconnect->setChecked(true);

        if (desktop_config.flags() & proto::control::BLOCK_REMOTE_INPUT)
            ui.checkbox_block_remote_input->setChecked(true);

        if (desktop_config.flags() & proto::control::ENABLE_CURSOR_SHAPE)
            ui.checkbox_cursor_shape->setChecked(true);

        if (desktop_config.flags() & proto::control::ENABLE_CLIPBOARD)
            ui.checkbox_clipboard->setChecked(true);
    }
    else
    {
        ui.groupbox_other->hide();
        ui.checkbox_cursor_shape->hide();
        ui.checkbox_clipboard->hide();
    }

    if (desktop_config.flags() & proto::control::CURSOR_POSITION)
        ui.checkbox_cursor_position->setChecked(true);

    if (desktop_config.flags() & proto::control::DISABLE_EFFECTS)
        ui.checkbox_desktop_effects->setChecked(true);

    if (desktop_config.flags() & proto::control::DISABLE_WALLPAPER)
        ui.checkbox_desktop_wallpaper->setChecked(true);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogDesktop::saveSettings(
    proto::peer::SessionType session_type, proto::address_book::ComputerGroupConfig* group_config)
{
    proto::control::Config* desktop_config;

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        desktop_config = group_config->mutable_session_config()->mutable_desktop_manage();
        group_config->mutable_inherit()->set_desktop_manage(ui.checkbox_inherit_config->isChecked());
    }
    else
    {
        DCHECK_EQ(session_type, proto::peer::SESSION_TYPE_DESKTOP_VIEW);
        desktop_config = group_config->mutable_session_config()->mutable_desktop_view();
        group_config->mutable_inherit()->set_desktop_view(ui.checkbox_inherit_config->isChecked());
    }

    proto::video::Encoding video_encoding =
        static_cast<proto::video::Encoding>(ui.combo_codec->currentData().toInt());

    desktop_config->set_video_encoding(video_encoding);

    quint32 flags = 0;

    if (ui.checkbox_audio->isChecked())
        desktop_config->set_audio_encoding(proto::audio::ENCODING_OPUS);
    else
        desktop_config->set_audio_encoding(proto::audio::ENCODING_UNKNOWN);

    if (ui.checkbox_cursor_shape->isChecked() && ui.checkbox_cursor_shape->isEnabled())
        flags |= proto::control::ENABLE_CURSOR_SHAPE;

    if (ui.checkbox_cursor_position->isChecked())
        flags |= proto::control::CURSOR_POSITION;

    if (ui.checkbox_clipboard->isChecked() && ui.checkbox_clipboard->isEnabled())
        flags |= proto::control::ENABLE_CLIPBOARD;

    if (ui.checkbox_desktop_effects->isChecked())
        flags |= proto::control::DISABLE_EFFECTS;

    if (ui.checkbox_desktop_wallpaper->isChecked())
        flags |= proto::control::DISABLE_WALLPAPER;

    if (ui.checkbox_block_remote_input->isChecked())
        flags |= proto::control::BLOCK_REMOTE_INPUT;

    if (ui.checkbox_lock_at_disconnect->isChecked())
        flags |= proto::control::LOCK_AT_DISCONNECT;

    desktop_config->set_flags(flags);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogDesktop::onCodecChanged(int item_index)
{
    proto::video::Encoding encoding =
        static_cast<proto::video::Encoding>(ui.combo_codec->itemData(item_index).toInt());
    LOG(INFO) << "[ACTION] Video encoding changed:" << encoding;
}

} // namespace console
