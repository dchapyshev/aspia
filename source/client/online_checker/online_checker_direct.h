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

namespace base {
class Location;
} // namespace base

namespace client {

class OnlineCheckerDirect final : public QObject
{
    Q_OBJECT

public:
    explicit OnlineCheckerDirect(QObject* parent = nullptr);
    ~OnlineCheckerDirect();

    struct Computer
    {
        int computer_id = -1;
        QString address;
        quint16 port = 0;
    };
    using ComputerList = QQueue<Computer>;

    void start(const ComputerList& computers);

signals:
    void sig_checkerResult(int computer_id, bool online);
    void sig_checkerFinished();

private:
    void onChecked(int computer_id, bool online);
    void onFinished(const base::Location& location);

    ComputerList pending_queue_;

    class Instance;
    QQueue<Instance*> work_queue_;
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_DIRECT_H
