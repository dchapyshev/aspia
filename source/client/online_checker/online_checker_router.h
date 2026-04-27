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

#include <QSet>
#include <QQueue>
#include <QTimer>

#include "base/peer/host_id.h"
#include "client/router_connection.h"

namespace base {
class Location;
} // namespace base

namespace client {

class OnlineCheckerRouter final : public QObject
{
    Q_OBJECT

public:
    struct Computer
    {
        qint64 computer_id = -1;
        qint64 router_id = -1;
        base::HostId host_id = base::kInvalidHostId;
    };
    using ComputerList = QQueue<Computer>;

    explicit OnlineCheckerRouter(const ComputerList& computers, QObject* parent = nullptr);
    ~OnlineCheckerRouter() final;

    void start();

signals:
    void sig_checkerResult(int computer_id, bool online);
    void sig_checkerFinished();

private slots:
    void onHostStatus(qint64 request_id, bool online);

private:
    void checkNextComputer();
    void onFinished(const base::Location& location);

    qint64 current_request_id_ = 0;
    QSet<qint64> routers_;
    QTimer timer_;

    ComputerList computers_;
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
