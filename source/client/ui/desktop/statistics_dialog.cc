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

#include <QTimer>

namespace client {

namespace {

QString capturerToString(quint32 type)
{
    switch (static_cast<base::ScreenCapturer::Type>(type))
    {
        case base::ScreenCapturer::Type::DEFAULT: return "DEFAULT";
        case base::ScreenCapturer::Type::FAKE: return "FAKE";
        case base::ScreenCapturer::Type::WIN_GDI: return "WIN_GDI";
        case base::ScreenCapturer::Type::WIN_DXGI: return "WIN_DXGI";
        case base::ScreenCapturer::Type::LINUX_X11: return "LINUX_X11";
        case base::ScreenCapturer::Type::MACOSX: return "MACOSX";
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
                item->setText(1, sizeToString(metrics.total_rx));
                break;

            case 2:
                item->setText(1, sizeToString(metrics.total_tx));
                break;

            case 3:
                item->setText(1, speedToString(metrics.speed_rx));
                break;

            case 4:
                item->setText(1, speedToString(metrics.speed_tx));
                break;

            case 5:
                item->setText(1, QString::number(metrics.video_packet_count));
                break;

            case 6:
                item->setText(1, QString("%1 / %2")
                              .arg(metrics.video_pause_count)
                              .arg(metrics.video_resume_count));
                break;

            case 7:
                item->setText(1, sizeToString(metrics.min_video_packet));
                break;

            case 8:
                item->setText(1, sizeToString(metrics.max_video_packet));
                break;

            case 9:
                item->setText(1, sizeToString(metrics.avg_video_packet));
                break;

            case 10:
                item->setText(1, QString::number(metrics.audio_packet_count));
                break;

            case 11:
                item->setText(1, QString("%1 / %2")
                              .arg(metrics.audio_pause_count)
                              .arg(metrics.audio_resume_count));
                break;

            case 12:
                item->setText(1, sizeToString(metrics.min_audio_packet));
                break;

            case 13:
                item->setText(1, sizeToString(metrics.max_audio_packet));
                break;

            case 14:
                item->setText(1, sizeToString(metrics.avg_audio_packet));
                break;

            case 15:
                item->setText(1, capturerToString(metrics.video_capturer_type));
                break;

            case 16:
                item->setText(1, QString::number(metrics.fps));
                break;

            case 17:
                item->setText(1, QString::number(metrics.send_mouse));
                break;

            case 18:
            {
                int total_mouse = metrics.send_mouse + metrics.drop_mouse;
                int percentage = 0;

                if (total_mouse != 0)
                    percentage = (metrics.drop_mouse * 100) / total_mouse;

                item->setText(1, QString("%1 (%2 %)").arg(metrics.drop_mouse).arg(percentage));
            }
            break;

            case 19:
                item->setText(1, QString::number(metrics.send_key));
                break;

            case 20:
                item->setText(1, QString::number(metrics.send_text));
                break;

            case 21:
                item->setText(1, QString::number(metrics.read_clipboard));
                break;

            case 22:
                item->setText(1, QString::number(metrics.send_clipboard));
                break;

            case 23:
                item->setText(1, QString::number(metrics.cursor_shape_count));
                break;

            case 24:
                item->setText(1, QString::number(metrics.cursor_taken_from_cache));
                break;

            case 25:
                item->setText(1, QString::number(metrics.cursor_cached));
                break;

            case 26:
                item->setText(1, QString::number(metrics.cursor_pos_count));
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString StatisticsDialog::sizeToString(qint64 size)
{
    static const qint64 kKB = 1024LL;
    static const qint64 kMB = kKB * 1024LL;
    static const qint64 kGB = kMB * 1024LL;
    static const qint64 kTB = kGB * 1024LL;

    QString units;
    qint64 divider;

    if (size >= kTB)
    {
        units = "TB";
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units = "GB";
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units = "MB";
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units = "kB";
        divider = kKB;
    }
    else
    {
        units = "B";
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(size) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

//--------------------------------------------------------------------------------------------------
// static
QString StatisticsDialog::speedToString(qint64 speed)
{
    static const qint64 kKB = 1024LL;
    static const qint64 kMB = kKB * 1024LL;
    static const qint64 kGB = kMB * 1024LL;
    static const qint64 kTB = kGB * 1024LL;

    QString units;
    qint64 divider;

    if (speed >= kTB)
    {
        units = "TB/s";
        divider = kTB;
    }
    else if (speed >= kGB)
    {
        units = "GB/s";
        divider = kGB;
    }
    else if (speed >= kMB)
    {
        units = "MB/s";
        divider = kMB;
    }
    else if (speed >= kKB)
    {
        units = "kB/s";
        divider = kKB;
    }
    else
    {
        units = "B/s";
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(speed) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

} // namespace client
