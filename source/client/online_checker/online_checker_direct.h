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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_DIRECT_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_DIRECT_H

#include <QObject>
#include <QQueue>

#include "client/config.h"

namespace base {
class Location;
} // namespace base

namespace client {

class OnlineCheckerDirect final : public QObject
{
    Q_OBJECT

public:
    using ComputerList = QQueue<ComputerConfig>;

    explicit OnlineCheckerDirect(const ComputerList& computers, QObject* parent = nullptr);
    ~OnlineCheckerDirect();

    void start();

signals:
    void sig_checkerResult(qint64 computer_id, bool online);
    void sig_checkerFinished();

private:
    void onChecked(qint64 computer_id, bool online);
    void onFinished(const base::Location& location);

    ComputerList pending_queue_;

    class Instance;
    QQueue<Instance*> work_queue_;
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_DIRECT_H
