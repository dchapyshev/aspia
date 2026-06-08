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

#ifndef CLIENT_UI_DESKTOP_STATISTICS_DIALOG_H
#define CLIENT_UI_DESKTOP_STATISTICS_DIALOG_H

#include <QDialog>
#include <QTime>

#include <memory>

#include "client/client_desktop.h"

namespace Ui {
class StatisticsDialog;
} // namespace Ui

class StatisticsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit StatisticsDialog(QWidget* parent = nullptr);
    ~StatisticsDialog() final;

    void setMetrics(const ClientDesktop::Metrics& metrics);
    void setMouseMetrics(int send_mouse, int drop_mouse);
    void setKeyMetrics(int send_key);
    void setTextMetrics(int send_text);

signals:
    void sig_metricsRequired();

private:
    std::unique_ptr<Ui::StatisticsDialog> ui;
    QTimer* update_timer_ = nullptr;
    QTime duration_;

    Q_DISABLE_COPY_MOVE(StatisticsDialog)
};

#endif // CLIENT_UI_DESKTOP_STATISTICS_DIALOG_H
