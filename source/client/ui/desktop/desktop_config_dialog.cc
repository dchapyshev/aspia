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

#include "client/ui/desktop/desktop_config_dialog.h"

#include <QPushButton>
#include <QTimer>

#include "base/logging.h"
#include "ui_desktop_config_dialog.h"

namespace client {

//--------------------------------------------------------------------------------------------------
DesktopConfigDialog::DesktopConfigDialog(proto::peer::SessionType session_type,
    const proto::desktop::Config& config, quint32 video_encodings, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::DesktopConfigDialog>()),
      config_(config)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    QPushButton* cancel_button = ui->button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    QComboBox* combo_codec = ui->combo_codec;

    if (video_encodings & proto::desktop::VIDEO_ENCODING_VP9)
        combo_codec->addItem("VP9", proto::desktop::VIDEO_ENCODING_VP9);

    if (video_encodings & proto::desktop::VIDEO_ENCODING_VP8)
        combo_codec->addItem("VP8", proto::desktop::VIDEO_ENCODING_VP8);

    int current_codec = combo_codec->findData(config_.video_encoding());
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);
    onCodecChanged(current_codec);

    if (config_.audio_encoding() != proto::audio::ENCODING_UNKNOWN)
        ui->checkbox_audio->setChecked(true);

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (config_.flags() & proto::desktop::LOCK_AT_DISCONNECT)
            ui->checkbox_lock_at_disconnect->setChecked(true);

        if (config_.flags() & proto::desktop::BLOCK_REMOTE_INPUT)
            ui->checkbox_block_remote_input->setChecked(true);

        if (config_.flags() & proto::desktop::ENABLE_CURSOR_SHAPE)
            ui->checkbox_cursor_shape->setChecked(true);

        if (config_.flags() & proto::desktop::ENABLE_CLIPBOARD)
            ui->checkbox_clipboard->setChecked(true);
    }
    else
    {
        ui->groupbox_other->hide();
        ui->checkbox_cursor_shape->hide();
        ui->checkbox_clipboard->hide();
    }

    if (config_.flags() & proto::desktop::CURSOR_POSITION)
        ui->checkbox_enable_cursor_pos->setChecked(true);

    if (config_.flags() & proto::desktop::DISABLE_EFFECTS)
        ui->checkbox_desktop_effects->setChecked(true);

    if (config_.flags() & proto::desktop::DISABLE_WALLPAPER)
        ui->checkbox_desktop_wallpaper->setChecked(true);

    connect(combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DesktopConfigDialog::onCodecChanged);

    connect(ui->button_box, &QDialogButtonBox::clicked,
            this, &DesktopConfigDialog::onButtonBoxClicked);

    QTimer::singleShot(0, this, [this]()
    {
        adjustSize();
        setFixedHeight(sizeHint().height());
    });
}

//--------------------------------------------------------------------------------------------------
DesktopConfigDialog::~DesktopConfigDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableAudioFeature(bool enable)
{
    LOG(INFO) << "enableAudioFeature:" << enable;
    ui->checkbox_audio->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableClipboardFeature(bool enable)
{
    LOG(INFO) << "enableClipboardFeature:" << enable;
    ui->checkbox_clipboard->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableCursorShapeFeature(bool enable)
{
    LOG(INFO) << "enableCursorShapeFeature:" << enable;
    ui->checkbox_cursor_shape->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableCursorPositionFeature(bool enable)
{
    LOG(INFO) << "enableCursorPositionFeature:" << enable;
    ui->checkbox_enable_cursor_pos->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableDesktopEffectsFeature(bool enable)
{
    LOG(INFO) << "enableDesktopEffectsFeature:" << enable;
    ui->checkbox_desktop_effects->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableDesktopWallpaperFeature(bool enable)
{
    LOG(INFO) << "enableDesktopWallpaperFeature:" << enable;
    ui->checkbox_desktop_wallpaper->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableLockAtDisconnectFeature(bool enable)
{
    LOG(INFO) << "enableLockAtDisconnectFeature:" << enable;
    ui->checkbox_lock_at_disconnect->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableBlockInputFeature(bool enable)
{
    LOG(INFO) << "enableBlockInputFeature:" << enable;
    ui->checkbox_block_remote_input->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::onCodecChanged(int item_index)
{
    proto::desktop::VideoEncoding encoding =
        static_cast<proto::desktop::VideoEncoding>(ui->combo_codec->itemData(item_index).toInt());
    LOG(INFO) << "[ACTION] Codec changed:" << encoding;
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        proto::desktop::VideoEncoding video_encoding =
            static_cast<proto::desktop::VideoEncoding>(ui->combo_codec->currentData().toInt());

        config_.set_video_encoding(video_encoding);

        if (ui->checkbox_audio->isChecked())
            config_.set_audio_encoding(proto::audio::ENCODING_OPUS);
        else
            config_.set_audio_encoding(proto::audio::ENCODING_UNKNOWN);

        quint32 flags = 0;

        if (ui->checkbox_cursor_shape->isChecked() && ui->checkbox_cursor_shape->isEnabled())
            flags |= proto::desktop::ENABLE_CURSOR_SHAPE;

        if (ui->checkbox_enable_cursor_pos->isChecked())
            flags |= proto::desktop::CURSOR_POSITION;

        if (ui->checkbox_clipboard->isChecked() && ui->checkbox_clipboard->isEnabled())
            flags |= proto::desktop::ENABLE_CLIPBOARD;

        if (ui->checkbox_desktop_effects->isChecked())
            flags |= proto::desktop::DISABLE_EFFECTS;

        if (ui->checkbox_desktop_wallpaper->isChecked())
            flags |= proto::desktop::DISABLE_WALLPAPER;

        if (ui->checkbox_block_remote_input->isChecked())
            flags |= proto::desktop::BLOCK_REMOTE_INPUT;

        if (ui->checkbox_lock_at_disconnect->isChecked())
            flags |= proto::desktop::LOCK_AT_DISCONNECT;

        config_.set_flags(flags);

        emit sig_configChanged(config_);
        accept();
    }
    else
    {
        LOG(INFO) << "[ACTION] Rejected by user";
    }
}

} // namespace client
