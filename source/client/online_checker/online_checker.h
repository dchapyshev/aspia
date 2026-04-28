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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H

#include <QObject>
#include <QList>

#include "client/config.h"
#include "client/online_checker/online_checker_direct.h"
#include "client/online_checker/online_checker_router.h"

namespace client {

class OnlineChecker final : public QObject
{
    Q_OBJECT

public:
    explicit OnlineChecker(QObject* parent = nullptr);
    ~OnlineChecker() final;

    using ComputerList = QList<ComputerConfig>;

    void start(const ComputerList& computers);

signals:
    void sig_checkerResult(qint64 computer_id, bool online);
    void sig_checkerFinished();

private slots:
    void onDirectCheckerResult(qint64 computer_id, bool online);
    void onDirectCheckerFinished();

    void onRouterCheckerResult(qint64 computer_id, bool online);
    void onRouterCheckerFinished();

private:
    OnlineCheckerDirect* direct_checker_ = nullptr;
    OnlineCheckerRouter* router_checker_ = nullptr;

    OnlineCheckerRouter::ComputerList router_computers_;
    OnlineCheckerDirect::ComputerList direct_computers_;

    bool direct_finished_ = false;
    bool router_finished_ = false;

    Q_DISABLE_COPY_MOVE(OnlineChecker)
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H
