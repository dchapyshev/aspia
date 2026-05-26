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

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>

#include "base/scoped_qpointer.h"
#include "client/config.h"
#include "client/online_checker/online_checker_direct.h"
#include "client/online_checker/online_checker_router.h"

// Persistent online-status oracle. Designed to live for the entire lifetime of the owning widget
// and serve multiple start() calls. Each result is cached for ~1 minute keyed by computer_id; if
// start() is called with a computer whose last check is still fresh, the cached value is emitted
// immediately (asynchronously) and no network probe is issued. Stale or absent entries trigger
// a real check via OnlineCheckerDirect/OnlineCheckerRouter. The "Refresh" UI action passes the
// visible computer ids through invalidate() to force a fresh probe.
class OnlineChecker final : public QObject
{
    Q_OBJECT

public:
    explicit OnlineChecker(QObject* parent = nullptr);
    ~OnlineChecker() final;

    using ComputerList = QList<ComputerConfig>;

    // Begin (or restart) checking the given computers. Cached fresh entries are emitted via
    // sig_checkerResult on the next event loop tick; stale ones go through a real probe. When all
    // results have been emitted, sig_checkerFinished is emitted exactly once. If start() is
    // called while a previous check is still running, the old in-flight work is discarded.
    void start(const ComputerList& computers);

    // Drops cached results for the given computers so a subsequent start() rechecks them.
    void invalidate(const QList<qint64>& computer_ids);

signals:
    void sig_checkerResult(qint64 computer_id, bool online);
    void sig_checkerFinished();

private slots:
    void onDirectCheckerResult(qint64 computer_id, bool online);
    void onDirectCheckerFinished();
    void onRouterCheckerResult(qint64 computer_id, bool online);
    void onRouterCheckerFinished();
    void emitPendingCached();

private:
    struct CacheEntry
    {
        bool online = false;
        QDateTime checked_at;
    };

    bool isCacheFresh(const CacheEntry& entry) const;
    void finishIfDone();

    QHash<qint64, CacheEntry> cache_;
    QList<QPair<qint64, bool>> pending_cached_hits_;

    ScopedQPointer<OnlineCheckerDirect> direct_checker_;
    ScopedQPointer<OnlineCheckerRouter> router_checker_;

    OnlineCheckerRouter::ComputerList router_computers_;
    OnlineCheckerDirect::ComputerList direct_computers_;

    bool direct_finished_ = true;
    bool router_finished_ = true;

    Q_DISABLE_COPY_MOVE(OnlineChecker)
};

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H
