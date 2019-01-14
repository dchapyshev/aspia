//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "client/config_factory.h"
#include "codec/video_util.h"
#include "ui_desktop_config_dialog.h"

namespace aspia {

namespace {

enum ColorDepth
{
    COLOR_DEPTH_ARGB,
    COLOR_DEPTH_RGB565,
    COLOR_DEPTH_RGB332,
    COLOR_DEPTH_RGB222,
    COLOR_DEPTH_RGB111
};

} // namespace

DesktopConfigDialog::DesktopConfigDialog(proto::SessionType session_type,
                                         const proto::desktop::Config& config,
                                         QWidget* parent)
    : QDialog(parent),
      ui(new Ui::DesktopConfigDialog()),
      config_(config)
{
    ui->setupUi(this);

    ConfigFactory::fixupDesktopConfig(&config_);

    QComboBox* combo_codec = ui->combo_codec;
    combo_codec->addItem(QStringLiteral("VP9"), QVariant(proto::desktop::VIDEO_ENCODING_VP9));
    combo_codec->addItem(QStringLiteral("VP8"), QVariant(proto::desktop::VIDEO_ENCODING_VP8));
    combo_codec->addItem(QStringLiteral("ZSTD"), QVariant(proto::desktop::VIDEO_ENCODING_ZSTD));

    int current_codec = combo_codec->findData(QVariant(config_.video_encoding()));
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);
    onCodecChanged(current_codec);

    QComboBox* combo_color_depth = ui->combo_color_depth;
    combo_color_depth->addItem(tr("True color (32 bit)"), QVariant(COLOR_DEPTH_ARGB));
    combo_color_depth->addItem(tr("High color (16 bit)"), QVariant(COLOR_DEPTH_RGB565));
    combo_color_depth->addItem(tr("256 colors (8 bit)"), QVariant(COLOR_DEPTH_RGB332));
    combo_color_depth->addItem(tr("64 colors (6 bit)"), QVariant(COLOR_DEPTH_RGB222));
    combo_color_depth->addItem(tr("8 colors (3 bit)"), QVariant(COLOR_DEPTH_RGB111));

    PixelFormat pixel_format = codec::VideoUtil::fromVideoPixelFormat(config_.pixel_format());
    ColorDepth color_depth = COLOR_DEPTH_ARGB;

    if (pixel_format.isEqual(PixelFormat::ARGB()))
        color_depth = COLOR_DEPTH_ARGB;
    else if (pixel_format.isEqual(PixelFormat::RGB565()))
        color_depth = COLOR_DEPTH_RGB565;
    else if (pixel_format.isEqual(PixelFormat::RGB332()))
        color_depth = COLOR_DEPTH_RGB332;
    else if (pixel_format.isEqual(PixelFormat::RGB222()))
        color_depth = COLOR_DEPTH_RGB222;
    else if (pixel_format.isEqual(PixelFormat::RGB111()))
        color_depth = COLOR_DEPTH_RGB111;

    int current_color_depth = combo_color_depth->findData(QVariant(color_depth));
    if (current_color_depth != -1)
        combo_color_depth->setCurrentIndex(current_color_depth);

    ui->slider_compression_ratio->setValue(config_.compress_ratio());
    onCompressionRatioChanged(config_.compress_ratio());

    ui->spin_scale_factor->setValue(config_.scale_factor());
    ui->spin_update_interval->setValue(config_.update_interval());

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (config_.flags() & proto::desktop::BLOCK_REMOTE_INPUT)
            ui->checkbox_block_remote_input->setChecked(true);

        if (config_.flags() & proto::desktop::ENABLE_CURSOR_SHAPE)
            ui->checkbox_cursor_shape->setChecked(true);

        if (config_.flags() & proto::desktop::ENABLE_CLIPBOARD)
            ui->checkbox_clipboard->setChecked(true);
    }
    else
    {
        ui->checkbox_block_remote_input->setEnabled(false);
        ui->checkbox_cursor_shape->setEnabled(false);
        ui->checkbox_clipboard->setEnabled(false);
    }

    if (config_.flags() & proto::desktop::DISABLE_DESKTOP_EFFECTS)
        ui->checkbox_desktop_effects->setChecked(true);

    if (config_.flags() & proto::desktop::DISABLE_DESKTOP_WALLPAPER)
        ui->checkbox_desktop_wallpaper->setChecked(true);

    connect(combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DesktopConfigDialog::onCodecChanged);

    connect(ui->slider_compression_ratio, &QSlider::valueChanged,
            this, &DesktopConfigDialog::onCompressionRatioChanged);

    connect(ui->button_box, &QDialogButtonBox::clicked,
            this, &DesktopConfigDialog::onButtonBoxClicked);

    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    QSize size_min = minimumSize();
    QSize size_hint = sizeHint();

    setFixedSize(QSize(std::max(size_min.width(), size_hint.width()),
                       std::max(size_min.height(), size_hint.height())));
}

DesktopConfigDialog::~DesktopConfigDialog() = default;

void DesktopConfigDialog::onCodecChanged(int item_index)
{
    bool has_pixel_format =
        (ui->combo_codec->itemData(item_index).toInt() == proto::desktop::VIDEO_ENCODING_ZSTD);

    ui->label_color_depth->setEnabled(has_pixel_format);
    ui->combo_color_depth->setEnabled(has_pixel_format);
    ui->label_compression_ratio->setEnabled(has_pixel_format);
    ui->slider_compression_ratio->setEnabled(has_pixel_format);
    ui->label_fast->setEnabled(has_pixel_format);
    ui->label_best->setEnabled(has_pixel_format);
}

void DesktopConfigDialog::onCompressionRatioChanged(int value)
{
    ui->label_compression_ratio->setText(tr("Compression ratio: %1").arg(value));
}

void DesktopConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        proto::desktop::VideoEncoding video_encoding =
            static_cast<proto::desktop::VideoEncoding>(ui->combo_codec->currentData().toInt());

        config_.set_video_encoding(video_encoding);

        if (video_encoding == proto::desktop::VIDEO_ENCODING_ZSTD)
        {
            PixelFormat pixel_format;

            switch (ui->combo_color_depth->currentData().toInt())
            {
                case COLOR_DEPTH_ARGB:
                    pixel_format = PixelFormat::ARGB();
                    break;

                case COLOR_DEPTH_RGB565:
                    pixel_format = PixelFormat::RGB565();
                    break;

                case COLOR_DEPTH_RGB332:
                    pixel_format = PixelFormat::RGB332();
                    break;

                case COLOR_DEPTH_RGB222:
                    pixel_format = PixelFormat::RGB222();
                    break;

                case COLOR_DEPTH_RGB111:
                    pixel_format = PixelFormat::RGB111();
                    break;

                default:
                    DLOG(LS_FATAL) << "Unexpected color depth";
                    break;
            }

            codec::VideoUtil::toVideoPixelFormat(pixel_format, config_.mutable_pixel_format());

            config_.set_compress_ratio(ui->slider_compression_ratio->value());
        }

        config_.set_scale_factor(ui->spin_scale_factor->value());
        config_.set_update_interval(ui->spin_update_interval->value());

        uint32_t flags = 0;

        if (ui->checkbox_cursor_shape->isChecked() && ui->checkbox_cursor_shape->isEnabled())
            flags |= proto::desktop::ENABLE_CURSOR_SHAPE;

        if (ui->checkbox_clipboard->isChecked() && ui->checkbox_clipboard->isEnabled())
            flags |= proto::desktop::ENABLE_CLIPBOARD;

        if (ui->checkbox_desktop_effects->isChecked())
            flags |= proto::desktop::DISABLE_DESKTOP_EFFECTS;

        if (ui->checkbox_desktop_wallpaper->isChecked())
            flags |= proto::desktop::DISABLE_DESKTOP_WALLPAPER;

        if (ui->checkbox_block_remote_input->isChecked())
            flags |= proto::desktop::BLOCK_REMOTE_INPUT;

        config_.set_flags(flags);

        emit configChanged(config_);
        accept();
    }
    else
    {
        reject();
    }

    close();
}

} // namespace aspia
