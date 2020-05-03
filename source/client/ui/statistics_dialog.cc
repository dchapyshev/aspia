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

#include "client/ui/statistics_dialog.h"

#include <QTimer>

namespace client {

StatisticsDialog::StatisticsDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    ui.tree->resizeColumnToContents(0);

    update_timer_ = new QTimer(this);
    connect(update_timer_, &QTimer::timeout, this, &StatisticsDialog::metricsRequired);
    update_timer_->start(std::chrono::milliseconds(500));
}

StatisticsDialog::~StatisticsDialog() = default;

void StatisticsDialog::setMetrics(const DesktopWindow::Metrics& metrics)
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = ui.tree->topLevelItem(i);

        switch (i)
        {
            case 0:
                item->setText(1, sizeToString(metrics.total_rx));
                break;

            case 1:
                item->setText(1, sizeToString(metrics.total_tx));
                break;

            case 2:
                item->setText(1, speedToString(metrics.speed_rx));
                break;

            case 3:
                item->setText(1, speedToString(metrics.speed_tx));
                break;

            case 4:
                item->setText(1, sizeToString(metrics.min_video_packet));
                break;

            case 5:
                item->setText(1, sizeToString(metrics.max_video_packet));
                break;

            case 6:
                item->setText(1, sizeToString(metrics.avg_video_packet));
                break;

            case 7:
                item->setText(1, QString::number(metrics.fps));
                break;
        }
    }
}

// static
QString StatisticsDialog::sizeToString(int64_t size)
{
    static const int64_t kKB = 1024LL;
    static const int64_t kMB = kKB * 1024LL;
    static const int64_t kGB = kMB * 1024LL;
    static const int64_t kTB = kGB * 1024LL;

    QString units;
    int64_t divider;

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

// static
QString StatisticsDialog::speedToString(int64_t speed)
{
    static const int64_t kKB = 1024LL;
    static const int64_t kMB = kKB * 1024LL;
    static const int64_t kGB = kMB * 1024LL;
    static const int64_t kTB = kGB * 1024LL;

    QString units;
    int64_t divider;

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
