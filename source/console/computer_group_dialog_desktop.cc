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

#include "console/computer_group_dialog_desktop.h"

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
ComputerGroupDialogDesktop::ComputerGroupDialogDesktop(int type, bool is_root_group, QWidget* parent)
    : ComputerGroupDialogTab(type, is_root_group, parent)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    connect(ui.combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComputerGroupDialogDesktop::onCodecChanged);

    connect(ui.slider_compress_ratio, &QSlider::valueChanged,
            this, &ComputerGroupDialogDesktop::onCompressionRatioChanged);

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
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogDesktop::restoreSettings(
    proto::SessionType session_type, const proto::address_book::ComputerGroupConfig& group_config)
{
    proto::desktop::Config desktop_config;

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        ui.checkbox_inherit_config->setChecked(group_config.inherit().desktop_manage());
        desktop_config = group_config.session_config().desktop_manage();
    }
    else
    {
        DCHECK_EQ(session_type, proto::SESSION_TYPE_DESKTOP_VIEW);

        ui.checkbox_inherit_config->setChecked(group_config.inherit().desktop_view());
        desktop_config = group_config.session_config().desktop_view();
    }

    QComboBox* combo_codec = ui.combo_codec;
    combo_codec->addItem("VP9", proto::desktop::VIDEO_ENCODING_VP9);
    combo_codec->addItem("VP8", proto::desktop::VIDEO_ENCODING_VP8);
    combo_codec->addItem("ZSTD", proto::desktop::VIDEO_ENCODING_ZSTD);

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

    base::PixelFormat pixel_format = base::PixelFormat::fromProto(desktop_config.pixel_format());
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

    if (desktop_config.audio_encoding() != proto::desktop::AUDIO_ENCODING_UNKNOWN)
        ui.checkbox_audio->setChecked(true);

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (desktop_config.flags() & proto::desktop::LOCK_AT_DISCONNECT)
            ui.checkbox_lock_at_disconnect->setChecked(true);

        if (desktop_config.flags() & proto::desktop::BLOCK_REMOTE_INPUT)
            ui.checkbox_block_remote_input->setChecked(true);

        if (desktop_config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE)
            ui.checkbox_cursor_shape->setChecked(true);

        if (desktop_config.flags() & proto::desktop::ENABLE_CLIPBOARD)
            ui.checkbox_clipboard->setChecked(true);

        if (desktop_config.flags() & proto::desktop::CLEAR_CLIPBOARD)
            ui.checkbox_clear_clipboard->setChecked(true);
    }
    else
    {
        ui.groupbox_other->hide();
        ui.checkbox_cursor_shape->hide();
        ui.checkbox_clipboard->hide();
        ui.checkbox_clear_clipboard->hide();
    }

    if (desktop_config.flags() & proto::desktop::CURSOR_POSITION)
        ui.checkbox_cursor_position->setChecked(true);

    if (desktop_config.flags() & proto::desktop::DISABLE_DESKTOP_EFFECTS)
        ui.checkbox_desktop_effects->setChecked(true);

    if (desktop_config.flags() & proto::desktop::DISABLE_DESKTOP_WALLPAPER)
        ui.checkbox_desktop_wallpaper->setChecked(true);

    if (desktop_config.flags() & proto::desktop::DISABLE_FONT_SMOOTHING)
        ui.checkbox_font_smoothing->setChecked(true);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogDesktop::saveSettings(
    proto::SessionType session_type, proto::address_book::ComputerGroupConfig* group_config)
{
    proto::desktop::Config* desktop_config;

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        desktop_config = group_config->mutable_session_config()->mutable_desktop_manage();
        group_config->mutable_inherit()->set_desktop_manage(ui.checkbox_inherit_config->isChecked());
    }
    else
    {
        DCHECK_EQ(session_type, proto::SESSION_TYPE_DESKTOP_VIEW);
        desktop_config = group_config->mutable_session_config()->mutable_desktop_view();
        group_config->mutable_inherit()->set_desktop_view(ui.checkbox_inherit_config->isChecked());
    }

    proto::desktop::VideoEncoding video_encoding =
        static_cast<proto::desktop::VideoEncoding>(ui.combo_codec->currentData().toInt());

    desktop_config->set_video_encoding(video_encoding);

    if (video_encoding == proto::desktop::VIDEO_ENCODING_ZSTD)
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

        desktop_config->mutable_pixel_format()->CopyFrom(pixel_format.toProto());
        desktop_config->set_compress_ratio(static_cast<quint32>(ui.slider_compress_ratio->value()));
    }

    quint32 flags = 0;

    if (ui.checkbox_audio->isChecked())
        desktop_config->set_audio_encoding(proto::desktop::AUDIO_ENCODING_OPUS);
    else
        desktop_config->set_audio_encoding(proto::desktop::AUDIO_ENCODING_UNKNOWN);

    if (ui.checkbox_cursor_shape->isChecked() && ui.checkbox_cursor_shape->isEnabled())
        flags |= proto::desktop::ENABLE_CURSOR_SHAPE;

    if (ui.checkbox_cursor_position->isChecked())
        flags |= proto::desktop::CURSOR_POSITION;

    if (ui.checkbox_clipboard->isChecked() && ui.checkbox_clipboard->isEnabled())
        flags |= proto::desktop::ENABLE_CLIPBOARD;

    if (ui.checkbox_desktop_effects->isChecked())
        flags |= proto::desktop::DISABLE_DESKTOP_EFFECTS;

    if (ui.checkbox_desktop_wallpaper->isChecked())
        flags |= proto::desktop::DISABLE_DESKTOP_WALLPAPER;

    if (ui.checkbox_font_smoothing->isChecked())
        flags |= proto::desktop::DISABLE_FONT_SMOOTHING;

    if (ui.checkbox_block_remote_input->isChecked())
        flags |= proto::desktop::BLOCK_REMOTE_INPUT;

    if (ui.checkbox_lock_at_disconnect->isChecked())
        flags |= proto::desktop::LOCK_AT_DISCONNECT;

    if (ui.checkbox_clear_clipboard->isChecked())
        flags |= proto::desktop::CLEAR_CLIPBOARD;

    desktop_config->set_flags(flags);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogDesktop::onCodecChanged(int item_index)
{
    proto::desktop::VideoEncoding encoding =
        static_cast<proto::desktop::VideoEncoding>(ui.combo_codec->itemData(item_index).toInt());

    LOG(LS_INFO) << "[ACTION] Video encoding changed: " << videoEncodingToString(encoding);

    bool has_pixel_format = (encoding == proto::desktop::VIDEO_ENCODING_ZSTD);

    ui.label_color_depth->setEnabled(has_pixel_format);
    ui.combobox_color_depth->setEnabled(has_pixel_format);
    ui.label_compress_ratio->setEnabled(has_pixel_format);
    ui.slider_compress_ratio->setEnabled(has_pixel_format);
    ui.label_fast->setEnabled(has_pixel_format);
    ui.label_best->setEnabled(has_pixel_format);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogDesktop::onCompressionRatioChanged(int value)
{
    LOG(LS_INFO) << "[ACTION] Compression ratio changed: " << value;
    ui.label_compress_ratio->setText(tr("Compression ratio: %1").arg(value));
}

} // namespace console
