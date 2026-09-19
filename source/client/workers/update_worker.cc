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

#include "base/build_config.h"
#include "base/logging.h"
#include "base/update/update_checker.h"
#include "client/database.h"

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
void UpdateWorker::onPrepare()
{
    Database& db = Database::instance();

    if (!db.isCheckUpdatesEnabled())
    {
        LOG(INFO) << "Update check is disabled";
        return;
    }

    update_checker_ = new UpdateChecker(db.updateChannel(), kClientUpdatePackage, this);

    connect(update_checker_, &UpdateChecker::sig_checkFinished,
            this, &UpdateWorker::onUpdateCheckFinished);
    connect(update_checker_, &UpdateChecker::sig_checkFailed,
            this, &UpdateWorker::onUpdateCheckFailed);
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onStart()
{
    if (!update_checker_)
        return;

    LOG(INFO) << "Start checking for updates";
    update_checker_->start();
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
void UpdateWorker::onUpdateCheckFinished(const UpdateInfo& update_info)
{
    update_checker_->disconnect(this);
    update_checker_.reset();

    if (!update_info.isValid())
    {
        LOG(INFO) << "No updates available";
        return;
    }

    LOG(INFO) << "New version available:" << update_info.version().toString();
    emit sig_updateAvailable(update_info);
}

//--------------------------------------------------------------------------------------------------
void UpdateWorker::onUpdateCheckFailed()
{
    update_checker_->disconnect(this);
    update_checker_.reset();

    LOG(ERROR) << "Error while retrieving update information";
}
