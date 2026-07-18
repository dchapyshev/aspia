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

#include "client/desktop/desktop/statistics_dialog.h"

#include <QTimer>

#include "base/logging.h"
#include "base/net/udp_channel.h"
#include "common/desktop/formatter.h"
#include "proto/desktop_video.h"
#include "ui_statistics_dialog.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString capturerToString(quint32 type)
{
    switch (static_cast<proto::video::ScreenCapturerType>(type))
    {
        case proto::video::SCREEN_CAPTURER_TYPE_FAKE: return "FAKE";
        case proto::video::SCREEN_CAPTURER_TYPE_WIN_GDI: return "WIN_GDI";
        case proto::video::SCREEN_CAPTURER_TYPE_WIN_DXGI: return "WIN_DXGI";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_X11: return "LINUX_X11";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_WAYLAND: return "LINUX_WAYLAND";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_KMS: return "LINUX_KMS";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_KWIN: return "LINUX_KWIN";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_WLR: return "LINUX_WLR";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_VT: return "LINUX_VT";
        case proto::video::SCREEN_CAPTURER_TYPE_MACOSX: return "MACOSX";
        case proto::video::SCREEN_CAPTURER_TYPE_ANDROID_MEDIA: return "ANDROID_MEDIA";
        default: return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
QString encoderToString(quint32 type, bool hardware_decoder)
{
    switch (static_cast<proto::video::Encoding>(type))
    {
        case proto::video::ENCODING_VP8: return "VP8";
        case proto::video::ENCODING_VP9: return "VP9";
        case proto::video::ENCODING_H264: return hardware_decoder ? "H264HW" : "H264SW";
        default: return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
QString udpMethodToString(UdpMethod method)
{
    switch (method)
    {
        case UdpMethod::DIRECT: return "Direct";
        case UdpMethod::GATEWAY_HOST: return "Gateway (host)";
        case UdpMethod::GATEWAY_CLIENT: return "Gateway (client)";
        case UdpMethod::HOLE_PUNCHING: return "Hole punching";
        default: return "Disabled";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
StatisticsDialog::StatisticsDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::StatisticsDialog>()),
      duration_(0, 0)
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);
    ui->tree->resizeColumnToContents(0);

    update_timer_ = new QTimer(this);
    connect(update_timer_, &QTimer::timeout, this, &StatisticsDialog::sig_metricsRequired);
    update_timer_->start(std::chrono::seconds(1));
}

//--------------------------------------------------------------------------------------------------
StatisticsDialog::~StatisticsDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::onNetworkMetrics(const NetworkWorker::Metrics& metrics)
{
    setValue(1, Formatter::sizeToString(metrics.total_tcp_rx) + " / " +
        Formatter::sizeToString(metrics.total_tcp_tx));
    setValue(2, Formatter::transferSpeedToString(metrics.speed_tcp_rx) + " / " +
        Formatter::transferSpeedToString(metrics.speed_tcp_tx));
    setValue(3, Formatter::sizeToString(metrics.total_udp_rx) + " / " +
        Formatter::sizeToString(metrics.total_udp_tx));
    setValue(4, Formatter::transferSpeedToString(metrics.speed_udp_rx) + " / " +
        Formatter::transferSpeedToString(metrics.speed_udp_tx));
    setValue(5, udpMethodToString(metrics.udp_method));
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::onVideoMetrics(const VideoWorker::Metrics& metrics)
{
    setValue(6, QString::number(metrics.packet_count));
    setValue(7, Formatter::sizeToString(metrics.min_packet) + " / " +
        Formatter::sizeToString(metrics.max_packet) + " / " + Formatter::sizeToString(metrics.avg_packet));
    setValue(10, capturerToString(metrics.capturer_type) + " / " +
        encoderToString(metrics.encoder_type, metrics.hardware_decoder));
    setValue(11, QString::number(metrics.fps));
    setValue(16, QString::number(metrics.cursor_shape_count) + " / " +
        QString::number(metrics.cursor_taken_from_cache));
    setValue(17, QString::number(metrics.cursor_cached));
    setValue(18, QString::number(metrics.cursor_pos_count));
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::onAudioMetrics(const AudioWorker::Metrics& metrics)
{
    setValue(8, QString::number(metrics.packet_count));
    setValue(9, Formatter::sizeToString(metrics.min_packet) + " / " +
        Formatter::sizeToString(metrics.max_packet) + " / " + Formatter::sizeToString(metrics.avg_packet));
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::setDuration(std::chrono::seconds duration)
{
    setValue(0, duration_.addSecs(static_cast<int>(duration.count())).toString());
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::setClipboardMetrics(int read_clipboard, int send_clipboard)
{
    setValue(15, QString::number(read_clipboard) + " / " + QString::number(send_clipboard));
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::setMouseMetrics(int send_mouse, int drop_mouse)
{
    int total_mouse = send_mouse + drop_mouse;
    int percentage = 0;

    if (total_mouse != 0)
        percentage = (drop_mouse * 100) / total_mouse;

    setValue(12, QString::number(send_mouse) + " / " +
        QString("%1 (%2 %)").arg(drop_mouse).arg(percentage));
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::setKeyMetrics(int send_key)
{
    setValue(13, QString::number(send_key));
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::setTextMetrics(int send_text)
{
    setValue(14, QString::number(send_text));
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::setValue(int row, const QString& value)
{
    ui->tree->topLevelItem(row)->setText(1, value);
}

