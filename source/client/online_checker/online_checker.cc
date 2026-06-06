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

#include "client/online_checker/online_checker.h"

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/config.h"

namespace {

constexpr qint64 kCacheSeconds = 3 * 60; // 3 minutes.

} // namespace

//--------------------------------------------------------------------------------------------------
OnlineChecker::OnlineChecker(QObject* parent)
    : QObject(parent),
      direct_thread_(Thread::AsioDispatcher)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
OnlineChecker::~OnlineChecker()
{
    LOG(TRACE) << "Dtor";

    if (router_checker_)
        router_checker_->disconnect(this);

    if (direct_checker_)
        direct_checker_->disconnect(this);
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::start(const HostList& hosts)
{
    LOG(TRACE) << "Start online checker (total hosts:" << hosts.size() << ")";

    // Cancel any in-flight check from a previous start() call. The old inner checkers are
    // disconnected so their late-arriving signals are ignored, and scheduled for deletion via
    // ScopedQPointer.
    if (router_checker_)
        router_checker_->disconnect(this);
    if (direct_checker_)
        direct_checker_->disconnect(this);
    router_checker_.reset();
    direct_checker_.reset();
    router_hosts_.clear();
    direct_hosts_.clear();
    router_finished_ = true;
    direct_finished_ = true;

    QList<QPair<qint64, bool>> cached_hits;

    for (const HostConfig& host : hosts)
    {
        const qint64 id = host.id();
        auto it = cache_.constFind(id);
        if (it != cache_.constEnd() && isCacheFresh(*it))
        {
            cached_hits.append({id, it->online});
            continue;
        }

        if (isHostId(host.address()))
            router_hosts_.emplace_back(host);
        else
            direct_hosts_.emplace_back(host);
    }

    if (!router_hosts_.isEmpty())
    {
        router_finished_ = false;
        router_checker_ = new OnlineCheckerRouter(router_hosts_, this);

        connect(router_checker_, &OnlineCheckerRouter::sig_checkerResult,
                this, &OnlineChecker::onRouterCheckerResult);
        connect(router_checker_, &OnlineCheckerRouter::sig_checkerFinished,
                this, &OnlineChecker::onRouterCheckerFinished);

        router_checker_->start();
    }

    if (!direct_hosts_.isEmpty())
    {
        direct_finished_ = false;

        if (!direct_thread_.isRunning())
            direct_thread_.start();

        direct_checker_ = new OnlineCheckerDirect(direct_hosts_);
        direct_checker_->moveToThread(&direct_thread_);

        connect(direct_checker_, &OnlineCheckerDirect::sig_checkerResult,
                this, &OnlineChecker::onDirectCheckerResult,
                Qt::QueuedConnection);
        connect(direct_checker_, &OnlineCheckerDirect::sig_checkerFinished,
                this, &OnlineChecker::onDirectCheckerFinished,
                Qt::QueuedConnection);

        QMetaObject::invokeMethod(direct_checker_, &OnlineCheckerDirect::start, Qt::QueuedConnection);
    }

    pending_cached_hits_ = std::move(cached_hits);
    if (!pending_cached_hits_.isEmpty() || (router_finished_ && direct_finished_))
    {
        // Either we have cached hits to emit, or everything was empty - in both cases dispatch
        // the cached-emit slot through the event loop so sig_checkerResult/sig_checkerFinished
        // are delivered asynchronously, after the caller's start() invocation returns.
        QMetaObject::invokeMethod(this, &OnlineChecker::emitPendingCached, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::invalidate(const QList<qint64>& entry_ids)
{
    for (qint64 id : entry_ids)
        cache_.remove(id);
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onDirectCheckerResult(qint64 entry_id, bool online)
{
    cache_.insert(entry_id, CacheEntry{online, QDateTime::currentDateTime()});
    emit sig_checkerResult(entry_id, online);
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onDirectCheckerFinished()
{
    direct_finished_ = true;
    LOG(TRACE) << "DIRECT checker finished (r:" << router_finished_ << ", d:" << direct_finished_ << ")";
    finishIfDone();
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onRouterCheckerResult(qint64 entry_id, bool online)
{
    cache_.insert(entry_id, CacheEntry{online, QDateTime::currentDateTime()});
    emit sig_checkerResult(entry_id, online);
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onRouterCheckerFinished()
{
    router_finished_ = true;
    LOG(TRACE) << "ROUTER checker finished (r:" << router_finished_ << ", d:" << direct_finished_ << ")";
    finishIfDone();
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::emitPendingCached()
{
    QList<QPair<qint64, bool>> hits = std::move(pending_cached_hits_);
    pending_cached_hits_.clear();

    for (const auto& pair : hits)
        emit sig_checkerResult(pair.first, pair.second);

    finishIfDone();
}

//--------------------------------------------------------------------------------------------------
bool OnlineChecker::isCacheFresh(const CacheEntry& entry) const
{
    if (!entry.checked_at.isValid())
        return false;
    return entry.checked_at.secsTo(QDateTime::currentDateTime()) < kCacheSeconds;
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::finishIfDone()
{
    if (router_finished_ && direct_finished_ && pending_cached_hits_.isEmpty())
        emit sig_checkerFinished();
}
