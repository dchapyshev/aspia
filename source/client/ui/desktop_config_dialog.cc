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

#include "client/ui/desktop_config_dialog.h"

#include "client/config_factory.h"

namespace client {

DesktopConfigDialog::DesktopConfigDialog(proto::SessionType session_type,
                                         const proto::DesktopConfig& config,
                                         uint32_t video_encodings,
                                         QWidget* parent)
    : QDialog(parent),
      config_(config)
{
    ui.setupUi(this);

    ConfigFactory::fixupDesktopConfig(&config_);

    QComboBox* combo_codec = ui.combo_codec;

    if (video_encodings & proto::VIDEO_ENCODING_VP9)
        combo_codec->addItem(QLatin1String("VP9"), proto::VIDEO_ENCODING_VP9);

    if (video_encodings & proto::VIDEO_ENCODING_VP8)
        combo_codec->addItem(QLatin1String("VP8"), proto::VIDEO_ENCODING_VP8);

    int current_codec = combo_codec->findData(config_.video_encoding());
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);

    if (config_.audio_encoding() != proto::AUDIO_ENCODING_UNKNOWN)
        ui.checkbox_audio->setChecked(true);

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (config_.flags() & proto::LOCK_AT_DISCONNECT)
            ui.checkbox_lock_at_disconnect->setChecked(true);

        if (config_.flags() & proto::BLOCK_REMOTE_INPUT)
            ui.checkbox_block_remote_input->setChecked(true);

        if (config_.flags() & proto::ENABLE_CURSOR_SHAPE)
            ui.checkbox_cursor_shape->setChecked(true);

        if (config_.flags() & proto::ENABLE_CLIPBOARD)
            ui.checkbox_clipboard->setChecked(true);
    }
    else
    {
        ui.groupbox_other->hide();
        ui.checkbox_cursor_shape->hide();
        ui.checkbox_clipboard->hide();
    }

    if (config_.flags() & proto::DISABLE_DESKTOP_EFFECTS)
        ui.checkbox_desktop_effects->setChecked(true);

    if (config_.flags() & proto::DISABLE_DESKTOP_WALLPAPER)
        ui.checkbox_desktop_wallpaper->setChecked(true);

    if (config_.flags() & proto::DISABLE_FONT_SMOOTHING)
        ui.checkbox_font_smoothing->setChecked(true);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &DesktopConfigDialog::onButtonBoxClicked);

    setFixedHeight(sizeHint().height());
}

DesktopConfigDialog::~DesktopConfigDialog() = default;

void DesktopConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        proto::VideoEncoding video_encoding =
            static_cast<proto::VideoEncoding>(ui.combo_codec->currentData().toInt());

        config_.set_video_encoding(video_encoding);

        if (ui.checkbox_audio->isChecked())
            config_.set_audio_encoding(proto::AUDIO_ENCODING_OPUS);
        else
            config_.set_audio_encoding(proto::AUDIO_ENCODING_UNKNOWN);

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

        config_.set_flags(flags);

        emit configChanged(config_);
        accept();
    }
}

} // namespace client
