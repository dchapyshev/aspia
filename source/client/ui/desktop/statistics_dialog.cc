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

#include "client/ui/desktop/statistics_dialog.h"

#include "base/logging.h"
#include "base/desktop/screen_capturer.h"
#include "common/ui/formatter.h"
#include "proto/desktop_video.h"

#include <QTimer>

namespace {

//--------------------------------------------------------------------------------------------------
QString capturerToString(quint32 type)
{
    switch (static_cast<ScreenCapturer::Type>(type))
    {
        case ScreenCapturer::Type::DEFAULT: return "DEFAULT";
        case ScreenCapturer::Type::FAKE: return "FAKE";
        case ScreenCapturer::Type::WIN_GDI: return "WIN_GDI";
        case ScreenCapturer::Type::WIN_DXGI: return "WIN_DXGI";
        case ScreenCapturer::Type::LINUX_X11: return "LINUX_X11";
        case ScreenCapturer::Type::MACOSX: return "MACOSX";
        default: return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
QString encoderToString(quint32 type)
{
    switch (static_cast<proto::video::Encoding>(type))
    {
        case proto::video::ENCODING_VP8: return "VP8";
        case proto::video::ENCODING_VP9: return "VP9";
        default: return "UNKNOWN";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
StatisticsDialog::StatisticsDialog(QWidget* parent)
    : QDialog(parent),
      duration_(0, 0)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);
    ui.tree->resizeColumnToContents(0);

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
void StatisticsDialog::setMetrics(const ClientDesktop::Metrics& metrics)
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = ui.tree->topLevelItem(i);

        switch (i)
        {
            case 0:
                item->setText(1, duration_.addSecs(
                    static_cast<int>(metrics.duration.count())).toString());
                break;
            case 1:
                item->setText(1, Formatter::sizeToString(metrics.total_tcp_rx) + " / " + Formatter::sizeToString(metrics.total_tcp_tx));
                break;
            case 2:
                item->setText(1, Formatter::transferSpeedToString(metrics.speed_tcp_rx) + " / " + Formatter::transferSpeedToString(metrics.speed_tcp_tx));
                break;
            case 3:
                item->setText(1, Formatter::sizeToString(metrics.total_udp_rx) + " / " + Formatter::sizeToString(metrics.total_udp_tx));
                break;
            case 4:
                item->setText(1, Formatter::transferSpeedToString(metrics.speed_udp_rx) + " / " + Formatter::transferSpeedToString(metrics.speed_udp_tx));
                break;
            case 5:
                item->setText(1, QString::number(metrics.video_packet_count));
                break;
            case 6:
                item->setText(1, Formatter::sizeToString(metrics.min_video_packet) + " / " +
                    Formatter::sizeToString(metrics.max_video_packet) + " / " + Formatter::sizeToString(metrics.avg_video_packet));
                break;
            case 7:
                item->setText(1, QString::number(metrics.audio_packet_count));
                break;
            case 8:
                item->setText(1, Formatter::sizeToString(metrics.min_audio_packet) + " / " +
                    Formatter::sizeToString(metrics.max_audio_packet) + " / " + Formatter::sizeToString(metrics.avg_audio_packet));
                break;
            case 9:
                item->setText(1, capturerToString(metrics.video_capturer_type) + " / " +
                    encoderToString(metrics.video_encoder_type));
                break;
            case 10:
                item->setText(1, QString::number(metrics.fps));
                break;
            case 11:
            {
                int total_mouse = metrics.send_mouse + metrics.drop_mouse;
                int percentage = 0;

                if (total_mouse != 0)
                    percentage = (metrics.drop_mouse * 100) / total_mouse;

                item->setText(1, QString::number(metrics.send_mouse) + " / " +
                    QString("%1 (%2 %)").arg(metrics.drop_mouse).arg(percentage));
            }
            break;
            case 12:
                item->setText(1, QString::number(metrics.send_key));
                break;
            case 13:
                item->setText(1, QString::number(metrics.send_text));
                break;
            case 14:
                item->setText(1, QString::number(metrics.read_clipboard) + " / " +
                    QString::number(metrics.send_clipboard));
                break;
            case 15:
                item->setText(1, QString::number(metrics.cursor_shape_count) + " / " +
                    QString::number(metrics.cursor_taken_from_cache));
                break;
            case 16:
                item->setText(1, QString::number(metrics.cursor_cached));
                break;
            case 17:
                item->setText(1, QString::number(metrics.cursor_pos_count));
                break;
        }
    }
}

