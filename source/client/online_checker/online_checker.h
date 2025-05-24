//
// Aspia Project
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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H

#include "base/macros_magic.h"
#include "base/thread.h"
#include "client/online_checker/online_checker_direct.h"
#include "client/online_checker/online_checker_router.h"

#include <optional>

#include <QObject>
#include <QList>

namespace client {

class OnlineChecker final : public QObject
{
    Q_OBJECT

public:
    explicit OnlineChecker(QObject* parent = nullptr);
    ~OnlineChecker() final;

    struct Computer
    {
        int computer_id = -1;
        QString address_or_id;
        quint16 port = 0;
    };
    using ComputerList = QList<Computer>;

    void checkComputers(const std::optional<RouterConfig>& router_config,
                        const ComputerList& computers);

signals:
    void sig_checkerResult(int computer_id, bool online);
    void sig_checkerFinished();

private slots:
    void onBeforeThreadRunning();
    void onAfterThreadRunning();
    void onDirectCheckerResult(int computer_id, bool online);
    void onDirectCheckerFinished();

    void onRouterCheckerResult(int computer_id, bool online);
    void onRouterCheckerFinished();

private:
    base::Thread io_thread_;

    OnlineCheckerDirect* direct_checker_ = nullptr;
    OnlineCheckerRouter* router_checker_ = nullptr;

    std::optional<RouterConfig> router_config_;
    OnlineCheckerRouter::ComputerList router_computers_;
    OnlineCheckerDirect::ComputerList direct_computers_;

    bool direct_finished_ = false;
    bool router_finished_ = false;

    DISALLOW_COPY_AND_ASSIGN(OnlineChecker);
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H
