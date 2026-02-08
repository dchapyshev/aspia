//
// SmartCafe Project
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

#ifndef CLIENT_UI_DESKTOP_STATISTICS_DIALOG_H
#define CLIENT_UI_DESKTOP_STATISTICS_DIALOG_H

#include <QTime>

#include "client/client_desktop.h"
#include "ui_statistics_dialog.h"

namespace client {

class StatisticsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit StatisticsDialog(QWidget* parent = nullptr);
    ~StatisticsDialog() final;

    void setMetrics(const ClientDesktop::Metrics& metrics);

signals:
    void sig_metricsRequired();

private:
    static QString sizeToString(qint64 size);
    static QString speedToString(qint64 speed);

    Ui::StatisticsDialog ui;
    QTimer* update_timer_ = nullptr;
    QTime duration_;

    Q_DISABLE_COPY(StatisticsDialog)
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_STATISTICS_DIALOG_H
