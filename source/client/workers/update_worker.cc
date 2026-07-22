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

#include "client/workers/update_worker.h"

#include "base/logging.h"
#include "base/version_constants.h"
#include "client/database.h"
#include "common/update_checker.h"

//--------------------------------------------------------------------------------------------------
UpdateWorker::UpdateWorker()
    : Worker(Thread::AsioDispatcher)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
UpdateWorker::~UpdateWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onStart()
{
#if defined(Q_OS_WINDOWS)
    Database& db = Database::instance();

    if (!db.isCheckUpdatesEnabled())
    {
        LOG(INFO) << "Update check is disabled";
        return;
    }

    update_checker_ = new UpdateChecker(db.updateServer(), "client", this);

    connect(update_checker_, &UpdateChecker::sig_checkedFinished,
            this, &UpdateWorker::onUpdateCheckedFinished);

    LOG(INFO) << "Start checking for updates";
    update_checker_->start();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onStop()
{
    if (update_checker_)
    {
        update_checker_->disconnect(this);
        update_checker_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onUpdateCheckedFinished(const QByteArray& result)
{
    update_checker_->disconnect(this);
    update_checker_.reset();

    if (result.isEmpty())
    {
        LOG(ERROR) << "Error while retrieving update information";
        return;
    }

    UpdateInfo update_info = UpdateInfo::fromXml(result);
    if (!update_info.isValid())
    {
        LOG(INFO) << "No updates available";
        return;
    }

    const QVersionNumber& current_version = kCurrentVersion;
    const QVersionNumber& update_version = update_info.version();

    if (update_version <= current_version)
    {
        LOG(INFO) << "No available updates";
        return;
    }

    LOG(INFO) << "New version available:" << update_version.toString();
    emit sig_updateAvailable(update_info);
}
