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

#include "console/computer_dialog_desktop.h"

#include "base/logging.h"
#include "base/desktop/pixel_format.h"

namespace console {

namespace {

enum ColorDepth
{
    COLOR_DEPTH_ARGB,
    COLOR_DEPTH_RGB565,
    COLOR_DEPTH_RGB332,
    COLOR_DEPTH_RGB222,
    COLOR_DEPTH_RGB111
};

//--------------------------------------------------------------------------------------------------
base::PixelFormat parsePixelFormat(const proto::PixelFormat& format)
{
    return base::PixelFormat(
        static_cast<quint8>(format.bits_per_pixel()),
        static_cast<quint16>(format.red_max()),
        static_cast<quint16>(format.green_max()),
        static_cast<quint16>(format.blue_max()),
        static_cast<quint8>(format.red_shift()),
        static_cast<quint8>(format.green_shift()),
        static_cast<quint8>(format.blue_shift()));
}

//--------------------------------------------------------------------------------------------------
void serializePixelFormat(const base::PixelFormat& from, proto::PixelFormat* to)
{
    to->set_bits_per_pixel(from.bitsPerPixel());

    to->set_red_max(from.redMax());
    to->set_green_max(from.greenMax());
    to->set_blue_max(from.blueMax());

    to->set_red_shift(from.redShift());
    to->set_green_shift(from.greenShift());
    to->set_blue_shift(from.blueShift());
}

const char* videoEncodingToString(proto::VideoEncoding encoding)
{
    switch (encoding)
    {
    case proto::VIDEO_ENCODING_ZSTD:
        return "VIDEO_ENCODING_ZSTD";
    case proto::VIDEO_ENCODING_VP8:
        return "VIDEO_ENCODING_VP8";
    case proto::VIDEO_ENCODING_VP9:
        return "VIDEO_ENCODING_VP9";
    default:
        return "Unknown";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ComputerDialogDesktop::ComputerDialogDesktop(int type, QWidget* parent)
    : ComputerDialogTab(type, parent)
{
    ui.setupUi(this);

    connect(ui.combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComputerDialogDesktop::onCodecChanged);

    connect(ui.slider_compress_ratio, &QSlider::valueChanged,
            this, &ComputerDialogDesktop::onCompressionRatioChanged);

    connect(ui.checkbox_inherit_config, &QCheckBox::toggled, this, [this](bool checked)
    {
        ui.groupbox_codec->setEnabled(!checked);
        ui.groupbox_features->setEnabled(!checked);
        ui.groupbox_appearance->setEnabled(!checked);
        ui.groupbox_other->setEnabled(!checked);
    });
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogDesktop::restoreSettings(
    proto::SessionType session_type, const proto::address_book::Computer& computer)
{
    proto::DesktopConfig desktop_config;

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        ui.checkbox_inherit_config->setChecked(computer.inherit().desktop_manage());
        desktop_config = computer.session_config().desktop_manage();
    }
    else
    {
        DCHECK_EQ(session_type, proto::SESSION_TYPE_DESKTOP_VIEW);

        ui.checkbox_inherit_config->setChecked(computer.inherit().desktop_view());
        desktop_config = computer.session_config().desktop_view();
    }

    QComboBox* combo_codec = ui.combo_codec;
    combo_codec->addItem("VP9", proto::VIDEO_ENCODING_VP9);
    combo_codec->addItem("VP8", proto::VIDEO_ENCODING_VP8);
    combo_codec->addItem("ZSTD", proto::VIDEO_ENCODING_ZSTD);

    QComboBox* combo_color_depth = ui.combobox_color_depth;
    combo_color_depth->addItem(tr("True color (32 bit)"), COLOR_DEPTH_ARGB);
    combo_color_depth->addItem(tr("High color (16 bit)"), COLOR_DEPTH_RGB565);
    combo_color_depth->addItem(tr("256 colors (8 bit)"), COLOR_DEPTH_RGB332);
    combo_color_depth->addItem(tr("64 colors (6 bit)"), COLOR_DEPTH_RGB222);
    combo_color_depth->addItem(tr("8 colors (3 bit)"), COLOR_DEPTH_RGB111);

    int current_codec = combo_codec->findData(desktop_config.video_encoding());
    if (current_codec == -1)
        current_codec = 0;

    combo_codec->setCurrentIndex(current_codec);
    onCodecChanged(current_codec);

    base::PixelFormat pixel_format = parsePixelFormat(desktop_config.pixel_format());
    ColorDepth color_depth;

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
    else
        color_depth = COLOR_DEPTH_ARGB;

    int current_color_depth = combo_color_depth->findData(color_depth);
    if (current_color_depth != -1)
        combo_color_depth->setCurrentIndex(current_color_depth);

    ui.slider_compress_ratio->setValue(static_cast<int>(desktop_config.compress_ratio()));
    onCompressionRatioChanged(static_cast<int>(desktop_config.compress_ratio()));

    if (desktop_config.audio_encoding() != proto::AUDIO_ENCODING_UNKNOWN)
        ui.checkbox_audio->setChecked(true);

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (desktop_config.flags() & proto::LOCK_AT_DISCONNECT)
            ui.checkbox_lock_at_disconnect->setChecked(true);

        if (desktop_config.flags() & proto::BLOCK_REMOTE_INPUT)
            ui.checkbox_block_remote_input->setChecked(true);

        if (desktop_config.flags() & proto::ENABLE_CURSOR_SHAPE)
            ui.checkbox_cursor_shape->setChecked(true);

        if (desktop_config.flags() & proto::ENABLE_CLIPBOARD)
            ui.checkbox_clipboard->setChecked(true);

        if (desktop_config.flags() & proto::CLEAR_CLIPBOARD)
            ui.checkbox_clear_clipboard->setChecked(true);
    }
    else
    {
        ui.groupbox_other->hide();
        ui.checkbox_cursor_shape->hide();
        ui.checkbox_clipboard->hide();
        ui.checkbox_clear_clipboard->hide();
    }

    if (desktop_config.flags() & proto::CURSOR_POSITION)
        ui.checkbox_cursor_position->setChecked(true);

    if (desktop_config.flags() & proto::DISABLE_DESKTOP_EFFECTS)
        ui.checkbox_desktop_effects->setChecked(true);

    if (desktop_config.flags() & proto::DISABLE_DESKTOP_WALLPAPER)
        ui.checkbox_desktop_wallpaper->setChecked(true);

    if (desktop_config.flags() & proto::DISABLE_FONT_SMOOTHING)
        ui.checkbox_font_smoothing->setChecked(true);
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogDesktop::saveSettings(
    proto::SessionType session_type, proto::address_book::Computer* computer)
{
    proto::DesktopConfig* desktop_config;

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        computer->mutable_inherit()->set_desktop_manage(ui.checkbox_inherit_config->isChecked());
        desktop_config = computer->mutable_session_config()->mutable_desktop_manage();
    }
    else
    {
        DCHECK_EQ(session_type, proto::SESSION_TYPE_DESKTOP_VIEW);

        computer->mutable_inherit()->set_desktop_view(ui.checkbox_inherit_config->isChecked());
        desktop_config = computer->mutable_session_config()->mutable_desktop_view();
    }
    proto::VideoEncoding video_encoding =
        static_cast<proto::VideoEncoding>(ui.combo_codec->currentData().toInt());

    desktop_config->set_video_encoding(video_encoding);

    if (video_encoding == proto::VIDEO_ENCODING_ZSTD)
    {
        base::PixelFormat pixel_format;

        switch (ui.combobox_color_depth->currentData().toInt())
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

        serializePixelFormat(pixel_format, desktop_config->mutable_pixel_format());

        desktop_config->set_compress_ratio(static_cast<quint32>(ui.slider_compress_ratio->value()));
    }

    quint32 flags = 0;

    if (ui.checkbox_audio->isChecked())
        desktop_config->set_audio_encoding(proto::AUDIO_ENCODING_OPUS);
    else
        desktop_config->set_audio_encoding(proto::AUDIO_ENCODING_UNKNOWN);

    if (ui.checkbox_cursor_shape->isChecked() && ui.checkbox_cursor_shape->isEnabled())
        flags |= proto::ENABLE_CURSOR_SHAPE;

    if (ui.checkbox_cursor_position->isChecked())
        flags |= proto::CURSOR_POSITION;

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

    if (ui.checkbox_clear_clipboard->isChecked())
        flags |= proto::CLEAR_CLIPBOARD;

    desktop_config->set_flags(flags);
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogDesktop::onCodecChanged(int item_index)
{
    proto::VideoEncoding encoding =
        static_cast<proto::VideoEncoding>(ui.combo_codec->itemData(item_index).toInt());

    LOG(LS_INFO) << "[ACTION] Video encoding changed: " << videoEncodingToString(encoding);

    bool has_pixel_format = (encoding == proto::VIDEO_ENCODING_ZSTD);

    ui.label_color_depth->setEnabled(has_pixel_format);
    ui.combobox_color_depth->setEnabled(has_pixel_format);
    ui.label_compress_ratio->setEnabled(has_pixel_format);
    ui.slider_compress_ratio->setEnabled(has_pixel_format);
    ui.label_fast->setEnabled(has_pixel_format);
    ui.label_best->setEnabled(has_pixel_format);
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogDesktop::onCompressionRatioChanged(int value)
{
    LOG(LS_INFO) << "[ACTION] Compression ratio changed: " << value;
    ui.label_compress_ratio->setText(tr("Compression ratio: %1").arg(value));
}

} // namespace console
