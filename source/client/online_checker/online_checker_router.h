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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H

#include <QObject>
#include <QQueue>

#include "client/config.h"

class Location;
class QTimer;

namespace proto::router {
class HostStatus;
} // namespace proto::router

class OnlineCheckerRouter final : public QObject
{
    Q_OBJECT

public:
    using HostList = QQueue<HostConfig>;

    explicit OnlineCheckerRouter(const HostList& hosts, QObject* parent = nullptr);
    ~OnlineCheckerRouter() final;

    void start();

signals:
    void sig_checkerResult(qint64 entry_id, bool online);
    void sig_checkerFinished();

private slots:
    void onHostStatusReceived(const proto::router::HostStatus& host_status);

private:
    void checkNextHost();
    void onFinished(const Location& location);

    QTimer* timer_ = nullptr;
    HostList hosts_;
};

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
