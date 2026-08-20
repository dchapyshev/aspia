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

#ifndef CLIENT_ROUTER_HOSTS_CLEANER_H
#define CLIENT_ROUTER_HOSTS_CLEANER_H

#include <QObject>

// Drops the credentials this machine keeps for hosts the router no longer has. Only the router
// knows a host is gone, so one pass asks about the rows left unchecked the longest and reports
// sig_finished() once the answers are in. Nothing happens until start() is called.
class RouterHostsCleaner final : public QObject
{
    Q_OBJECT

public:
    explicit RouterHostsCleaner(qint64 router_id, QObject* parent = nullptr);
    ~RouterHostsCleaner() final;

    void start();

signals:
    void sig_finished();

private:
    const qint64 router_id_;
    qsizetype pending_ = 0;
    bool started_ = false;

    Q_DISABLE_COPY_MOVE(RouterHostsCleaner)
};

#endif // CLIENT_ROUTER_HOSTS_CLEANER_H
