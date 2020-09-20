//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/computer_dialog_desktop.h"

namespace console {

ComputerDialogDesktop::ComputerDialogDesktop(int type, QWidget* parent)
    : ComputerDialogTab(type, parent)
{
    ui.setupUi(this);
}

void ComputerDialogDesktop::restoreSettings(
    proto::SessionType session_type, const proto::DesktopConfig& config)
{
    QComboBox* combo_codec = ui.combo_codec;
    combo_codec->addItem(QLatin1String("VP9"), proto::VIDEO_ENCODING_VP9);
    combo_codec->addItem(QLatin1String("VP8"), proto::VIDEO_ENCODING_VP8);

    int current_codec = combo_codec->findData(config.video_encoding());
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (config.flags() & proto::LOCK_AT_DISCONNECT)
            ui.checkbox_lock_at_disconnect->setChecked(true);

        if (config.flags() & proto::BLOCK_REMOTE_INPUT)
            ui.checkbox_block_remote_input->setChecked(true);

        if (config.flags() & proto::ENABLE_CURSOR_SHAPE)
            ui.checkbox_cursor_shape->setChecked(true);

        if (config.flags() & proto::ENABLE_CLIPBOARD)
            ui.checkbox_clipboard->setChecked(true);
    }
    else
    {
        ui.checkbox_lock_at_disconnect->hide();
        ui.checkbox_block_remote_input->hide();
        ui.checkbox_cursor_shape->hide();
        ui.checkbox_clipboard->hide();
    }

    if (config.flags() & proto::DISABLE_DESKTOP_EFFECTS)
        ui.checkbox_desktop_effects->setChecked(true);

    if (config.flags() & proto::DISABLE_DESKTOP_WALLPAPER)
        ui.checkbox_desktop_wallpaper->setChecked(true);

    if (config.flags() & proto::DISABLE_FONT_SMOOTHING)
        ui.checkbox_font_smoothing->setChecked(true);
}

void ComputerDialogDesktop::saveSettings(proto::DesktopConfig* config)
{
    proto::VideoEncoding video_encoding =
        static_cast<proto::VideoEncoding>(ui.combo_codec->currentData().toInt());

    config->set_video_encoding(video_encoding);

    uint32_t flags = 0;

    if (ui.checkbox_cursor_shape->isChecked() && ui.checkbox_cursor_shape->isEnabled())
        flags |= proto::ENABLE_CURSOR_SHAPE;

    if (ui.checkbox_clipboard->isChecked() && ui.checkbox_clipboard->isEnabled())
        flags |= proto::ENABLE_CLIPBOARD;

    if (ui.checkbox_desktop_effects->isChecked())
        flags |= proto::DISABLE_DESKTOP_EFFECTS;

    if (ui.checkbox_desktop_wallpaper->isChecked())
        flags |= proto::DISABLE_DESKTOP_WALLPAPER;

    if (ui.checkbox_font_smoothing->isChecked())
        flags |= proto::DISABLE_FONT_SMOOTHING;

    if (ui.checkbox_block_remote_input->isChecked())
        flags |= proto::BLOCK_REMOTE_INPUT;

    if (ui.checkbox_lock_at_disconnect->isChecked())
        flags |= proto::LOCK_AT_DISCONNECT;

    config->set_flags(flags);
}

} // namespace console
