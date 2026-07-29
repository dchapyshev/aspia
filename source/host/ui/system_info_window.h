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

#ifndef HOST_UI_SYSTEM_INFO_WINDOW_H
#define HOST_UI_SYSTEM_INFO_WINDOW_H

#include <QWidget>

namespace proto::system_info {
class SystemInfoRequest;
} // namespace proto::system_info

class SysInfoView;

// Shows the information about the computer the host runs on. Unlike the client, which asks a remote
// host over the network, the report is built in this process.
class SystemInfoWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit SystemInfoWindow(QWidget* parent = nullptr);
    ~SystemInfoWindow() final;

signals:
    void sig_query(quint32 consumer_id, const QByteArray& buffer);

protected:
    // QWidget implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request);
    void onSystemInfo(quint32 consumer_id, const QByteArray& buffer);

private:
    // Identifies the queries of this window in the worker.
    const quint32 consumer_id_;

    SysInfoView* view_ = nullptr;

    Q_DISABLE_COPY_MOVE(SystemInfoWindow)
};

#endif // HOST_UI_SYSTEM_INFO_WINDOW_H
