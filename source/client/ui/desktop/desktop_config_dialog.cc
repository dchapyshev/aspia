//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "base/desktop/pixel_format.h"
#include "client/config_factory.h"
#include "ui_desktop_config_dialog.h"

#include <QPushButton>
#include <QTimer>

namespace client {

namespace {

enum ColorDepth
{
    COLOR_DEPTH_ARGB,
    COLOR_DEPTH_RGB565,
    COLOR_DEPTH_RGB332,
    COLOR_DEPTH_RGB222,
    COLOR_DEPTH_RGB111
};

const char* videoEncodingToString(proto::desktop::VideoEncoding encoding)
{
    switch (encoding)
    {
        case proto::desktop::VIDEO_ENCODING_ZSTD:
            return "VIDEO_ENCODING_ZSTD";
        case proto::desktop::VIDEO_ENCODING_VP8:
            return "VIDEO_ENCODING_VP8";
        case proto::desktop::VIDEO_ENCODING_VP9:
            return "VIDEO_ENCODING_VP9";
        default:
            return "Unknown";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopConfigDialog::DesktopConfigDialog(proto::peer::SessionType session_type,
                                         const proto::desktop::Config& config,
                                         quint32 video_encodings,
                                         QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::DesktopConfigDialog>()),
      config_(config)
{
    LOG(LS_INFO) << "Ctor";
    ui->setupUi(this);

    QPushButton* cancel_button = ui->button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    ConfigFactory::fixupDesktopConfig(&config_);

    QComboBox* combo_codec = ui->combo_codec;

    if (video_encodings & proto::desktop::VIDEO_ENCODING_VP9)
        combo_codec->addItem("VP9", proto::desktop::VIDEO_ENCODING_VP9);

    if (video_encodings & proto::desktop::VIDEO_ENCODING_VP8)
        combo_codec->addItem("VP8", proto::desktop::VIDEO_ENCODING_VP8);

    if (video_encodings & proto::desktop::VIDEO_ENCODING_ZSTD)
        combo_codec->addItem("ZSTD", proto::desktop::VIDEO_ENCODING_ZSTD);

    int current_codec = combo_codec->findData(config_.video_encoding());
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);
    onCodecChanged(current_codec);

    QComboBox* combo_color_depth = ui->combobox_color_depth;
    combo_color_depth->addItem(tr("True color (32 bit)"), COLOR_DEPTH_ARGB);
    combo_color_depth->addItem(tr("High color (16 bit)"), COLOR_DEPTH_RGB565);
    combo_color_depth->addItem(tr("256 colors (8 bit)"), COLOR_DEPTH_RGB332);
    combo_color_depth->addItem(tr("64 colors (6 bit)"), COLOR_DEPTH_RGB222);
    combo_color_depth->addItem(tr("8 colors (3 bit)"), COLOR_DEPTH_RGB111);

    base::PixelFormat pixel_format = base::PixelFormat::fromProto(config_.pixel_format());
    ColorDepth color_depth = COLOR_DEPTH_ARGB;

    if (pixel_format.isEqual(base::PixelFormat::ARGB()))
        color_depth = COLOR_DEPTH_ARGB;
    else if (pixel_format.isEqual(base::PixelFormat::RGB565()))
        color_depth = COLOR_DEPTH_RGB565;
    else if (pixel_format.isEqual(base::PixelFormat::RGB332()))
        color_depth = COLOR_DEPTH_RGB332;
    else if (pixel_format.isEqual(base::PixelFormat::RGB222()))
        color_depth = COLOR_DEPTH_RGB222;
    else if (pixel_format.isEqual(base::PixelFormat::RGB111()))
        color_depth = COLOR_DEPTH_RGB111;

    int current_color_depth = combo_color_depth->findData(QVariant(color_depth));
    if (current_color_depth != -1)
        combo_color_depth->setCurrentIndex(current_color_depth);

    ui->slider_compress_ratio->setValue(static_cast<int>(config_.compress_ratio()));
    onCompressionRatioChanged(static_cast<int>(config_.compress_ratio()));

    if (config_.audio_encoding() != proto::desktop::AUDIO_ENCODING_UNKNOWN)
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

        if (config_.flags() & proto::desktop::CLEAR_CLIPBOARD)
            ui->checkbox_clear_clipboard->setChecked(true);
    }
    else
    {
        ui->groupbox_other->hide();
        ui->checkbox_cursor_shape->hide();
        ui->checkbox_clipboard->hide();
        ui->checkbox_clear_clipboard->hide();
    }

    if (config_.flags() & proto::desktop::CURSOR_POSITION)
        ui->checkbox_enable_cursor_pos->setChecked(true);

    if (config_.flags() & proto::desktop::DISABLE_EFFECTS)
        ui->checkbox_desktop_effects->setChecked(true);

    if (config_.flags() & proto::desktop::DISABLE_WALLPAPER)
        ui->checkbox_desktop_wallpaper->setChecked(true);

    if (config_.flags() & proto::desktop::DISABLE_FONT_SMOOTHING)
        ui->checkbox_font_smoothing->setChecked(true);

    connect(combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DesktopConfigDialog::onCodecChanged);

    connect(ui->slider_compress_ratio, &QSlider::valueChanged,
            this, &DesktopConfigDialog::onCompressionRatioChanged);

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
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableAudioFeature(bool enable)
{
    LOG(LS_INFO) << "enableAudioFeature:" << enable;
    ui->checkbox_audio->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableClipboardFeature(bool enable)
{
    LOG(LS_INFO) << "enableClipboardFeature:" << enable;
    ui->checkbox_clipboard->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableCursorShapeFeature(bool enable)
{
    LOG(LS_INFO) << "enableCursorShapeFeature:" << enable;
    ui->checkbox_cursor_shape->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableCursorPositionFeature(bool enable)
{
    LOG(LS_INFO) << "enableCursorPositionFeature:" << enable;
    ui->checkbox_enable_cursor_pos->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableDesktopEffectsFeature(bool enable)
{
    LOG(LS_INFO) << "enableDesktopEffectsFeature:" << enable;
    ui->checkbox_desktop_effects->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableDesktopWallpaperFeature(bool enable)
{
    LOG(LS_INFO) << "enableDesktopWallpaperFeature:" << enable;
    ui->checkbox_desktop_wallpaper->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableFontSmoothingFeature(bool enable)
{
    LOG(LS_INFO) << "enableFontSmoothingFeature:" << enable;
    ui->checkbox_font_smoothing->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableClearClipboardFeature(bool enable)
{
    LOG(LS_INFO) << "enableClearClipboardFeature:" << enable;
    ui->checkbox_clear_clipboard->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableLockAtDisconnectFeature(bool enable)
{
    LOG(LS_INFO) << "enableLockAtDisconnectFeature:" << enable;
    ui->checkbox_lock_at_disconnect->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableBlockInputFeature(bool enable)
{
    LOG(LS_INFO) << "enableBlockInputFeature:" << enable;
    ui->checkbox_block_remote_input->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::onCodecChanged(int item_index)
{
    proto::desktop::VideoEncoding encoding =
        static_cast<proto::desktop::VideoEncoding>(ui->combo_codec->itemData(item_index).toInt());

    LOG(LS_INFO) << "[ACTION] Codec changed:" << videoEncodingToString(encoding);

    bool has_pixel_format = (encoding == proto::desktop::VIDEO_ENCODING_ZSTD);

    ui->label_color_depth->setEnabled(has_pixel_format);
    ui->combobox_color_depth->setEnabled(has_pixel_format);
    ui->label_compress_ratio->setEnabled(has_pixel_format);
    ui->slider_compress_ratio->setEnabled(has_pixel_format);
    ui->label_fast->setEnabled(has_pixel_format);
    ui->label_best->setEnabled(has_pixel_format);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::onCompressionRatioChanged(int value)
{
    LOG(LS_INFO) << "[ACTION] Compression ratio changed:" << value;
    ui->label_compress_ratio->setText(tr("Compression ratio: %1").arg(value));
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(LS_INFO) << "[ACTION] Accepted by user";

        proto::desktop::VideoEncoding video_encoding =
            static_cast<proto::desktop::VideoEncoding>(ui->combo_codec->currentData().toInt());

        config_.set_video_encoding(video_encoding);

        if (video_encoding == proto::desktop::VIDEO_ENCODING_ZSTD)
        {
            base::PixelFormat pixel_format;

            switch (ui->combobox_color_depth->currentData().toInt())
            {
                case COLOR_DEPTH_ARGB:
                    pixel_format = base::PixelFormat::ARGB();
                    break;

                case COLOR_DEPTH_RGB565:
                    pixel_format = base::PixelFormat::RGB565();
                    break;

                case COLOR_DEPTH_RGB332:
                    pixel_format = base::PixelFormat::RGB332();
                    break;

                case COLOR_DEPTH_RGB222:
                    pixel_format = base::PixelFormat::RGB222();
                    break;

                case COLOR_DEPTH_RGB111:
                    pixel_format = base::PixelFormat::RGB111();
                    break;

                default:
                    DLOG(LS_FATAL) << "Unexpected color depth";
                    break;
            }

            config_.mutable_pixel_format()->CopyFrom(pixel_format.toProto());
            config_.set_compress_ratio(static_cast<quint32>(ui->slider_compress_ratio->value()));
        }

        if (ui->checkbox_audio->isChecked())
            config_.set_audio_encoding(proto::desktop::AUDIO_ENCODING_OPUS);
        else
            config_.set_audio_encoding(proto::desktop::AUDIO_ENCODING_UNKNOWN);

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

        if (ui->checkbox_font_smoothing->isChecked())
            flags |= proto::desktop::DISABLE_FONT_SMOOTHING;

        if (ui->checkbox_block_remote_input->isChecked())
            flags |= proto::desktop::BLOCK_REMOTE_INPUT;

        if (ui->checkbox_lock_at_disconnect->isChecked())
            flags |= proto::desktop::LOCK_AT_DISCONNECT;

        if (ui->checkbox_clear_clipboard->isChecked())
            flags |= proto::desktop::CLEAR_CLIPBOARD;

        config_.set_flags(flags);

        emit sig_configChanged(config_);
        accept();
    }
    else
    {
        LOG(LS_INFO) << "[ACTION] Rejected by user";
    }
}

} // namespace client
