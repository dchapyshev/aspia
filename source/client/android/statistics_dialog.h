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

#ifndef CLIENT_ANDROID_STATISTICS_DIALOG_H
#define CLIENT_ANDROID_STATISTICS_DIALOG_H

#include <QList>

#include <chrono>

#include "client/workers/audio_worker.h"
#include "client/workers/network_worker.h"
#include "client/workers/video_worker.h"
#include "common/android/dialog.h"

class QTreeWidgetItem;
class TreeWidget;

class StatisticsDialog final : public Dialog
{
    Q_OBJECT

public:
    explicit StatisticsDialog(QWidget* parent = nullptr);
    ~StatisticsDialog() final;

    void setDuration(std::chrono::seconds duration);
    void setClipboardMetrics(int read_clipboard, int send_clipboard);

public slots:
    void onNetworkMetrics(const NetworkWorker::Metrics& metrics);
    void onVideoMetrics(const VideoWorker::Metrics& metrics);
    void onAudioMetrics(const AudioWorker::Metrics& metrics);

signals:
    // Emitted once a second to ask the owner to refresh the session-level metrics.
    void sig_metricsRequired();

private:
    QTreeWidgetItem* addRow(const QString& name);

    TreeWidget* tree_ = nullptr;
    QList<QTreeWidgetItem*> rows_;

    Q_DISABLE_COPY_MOVE(StatisticsDialog)
};

#endif // CLIENT_ANDROID_STATISTICS_DIALOG_H
