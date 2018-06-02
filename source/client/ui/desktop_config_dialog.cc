//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_config_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/desktop_config_dialog.h"

#include "codec/video_util.h"

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

DesktopConfigDialog::DesktopConfigDialog(proto::desktop::Config* config,
                                         quint32 supported_video_encodings,
                                         quint32 supported_features,
                                         QWidget* parent)
    : QDialog(parent),
      supported_video_encodings_(supported_video_encodings),
      supported_features_(supported_features),
      config_(config)
{
    ui.setupUi(this);

    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setFixedSize(size());

    if (supported_video_encodings_ & proto::desktop::VIDEO_ENCODING_VP9)
        ui.combo_codec->addItem(tr("VP9 (LossLess)"), QVariant(proto::desktop::VIDEO_ENCODING_VP9));

    if (supported_video_encodings_ & proto::desktop::VIDEO_ENCODING_VP8)
        ui.combo_codec->addItem(tr("VP8"), QVariant(proto::desktop::VIDEO_ENCODING_VP8));

    if (supported_video_encodings_ & proto::desktop::VIDEO_ENCODING_ZLIB)
        ui.combo_codec->addItem(tr("ZLIB"), QVariant(proto::desktop::VIDEO_ENCODING_ZLIB));

    int current_codec = ui.combo_codec->findData(QVariant(config->video_encoding()));
    if (current_codec == -1)
        current_codec = 0;

    ui.combo_codec->setCurrentIndex(current_codec);
    OnCodecChanged(current_codec);

    ui.combo_color_depth->addItem(tr("True color (32 bit)"), QVariant(COLOR_DEPTH_ARGB));
    ui.combo_color_depth->addItem(tr("High color (16 bit)"), QVariant(COLOR_DEPTH_RGB565));
    ui.combo_color_depth->addItem(tr("256 colors (8 bit)"), QVariant(COLOR_DEPTH_RGB332));
    ui.combo_color_depth->addItem(tr("64 colors (6 bit)"), QVariant(COLOR_DEPTH_RGB222));
    ui.combo_color_depth->addItem(tr("8 colors (3 bit)"), QVariant(COLOR_DEPTH_RGB111));

    PixelFormat pixel_format = VideoUtil::fromVideoPixelFormat(config->pixel_format());
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

    int current_color_depth = ui.combo_color_depth->findData(QVariant(color_depth));
    if (current_color_depth != -1)
        ui.combo_color_depth->setCurrentIndex(current_color_depth);

    ui.slider_compression_ratio->setValue(config->compress_ratio());
    OnCompressionRatioChanged(config->compress_ratio());

    ui.spin_update_interval->setValue(config->update_interval());

    if (config->features() & proto::desktop::FEATURE_CURSOR_SHAPE)
        ui.checkbox_cursor_shape->setChecked(true);

    if (!(supported_features_ & proto::desktop::FEATURE_CURSOR_SHAPE))
        ui.checkbox_cursor_shape->setEnabled(false);

    if (config->features() & proto::desktop::FEATURE_CLIPBOARD)
        ui.checkbox_clipboard->setChecked(true);

    if (!(supported_features_ & proto::desktop::FEATURE_CLIPBOARD))
        ui.checkbox_clipboard->setEnabled(false);

    connect(ui.combo_codec, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DesktopConfigDialog::OnCodecChanged);

    connect(ui.slider_compression_ratio, &QSlider::valueChanged,
            this, &DesktopConfigDialog::OnCompressionRatioChanged);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &DesktopConfigDialog::OnButtonBoxClicked);
}

DesktopConfigDialog::~DesktopConfigDialog() = default;

void DesktopConfigDialog::OnCodecChanged(int item_index)
{
    bool has_pixel_format =
        (ui.combo_codec->itemData(item_index).toInt() == proto::desktop::VIDEO_ENCODING_ZLIB);

    ui.label_color_depth->setEnabled(has_pixel_format);
    ui.combo_color_depth->setEnabled(has_pixel_format);
    ui.label_compression_ratio->setEnabled(has_pixel_format);
    ui.slider_compression_ratio->setEnabled(has_pixel_format);
    ui.label_fast->setEnabled(has_pixel_format);
    ui.label_best->setEnabled(has_pixel_format);
}

void DesktopConfigDialog::OnCompressionRatioChanged(int value)
{
    ui.label_compression_ratio->setText(QString(tr("Compression ratio: %1").arg(value)));
}

void DesktopConfigDialog::OnButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        proto::desktop::VideoEncoding video_encoding =
            static_cast<proto::desktop::VideoEncoding>(ui.combo_codec->currentData().toInt());

        config_->set_video_encoding(video_encoding);

        if (video_encoding == proto::desktop::VIDEO_ENCODING_ZLIB)
        {
            PixelFormat pixel_format;

            switch (ui.combo_color_depth->currentData().toInt())
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
                    qFatal("Unexpected color depth");
                    break;
            }

            VideoUtil::toVideoPixelFormat(pixel_format, config_->mutable_pixel_format());

            config_->set_compress_ratio(ui.slider_compression_ratio->value());
        }

        config_->set_update_interval(ui.spin_update_interval->value());

        quint32 features = 0;

        if (ui.checkbox_cursor_shape->isChecked())
            features |= proto::desktop::FEATURE_CURSOR_SHAPE;

        if (ui.checkbox_clipboard->isChecked())
            features |= proto::desktop::FEATURE_CLIPBOARD;

        config_->set_features(features);

        accept();
    }
    else
    {
        reject();
    }

    close();
}

} // namespace aspia
