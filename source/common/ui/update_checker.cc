//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/ui/update_checker.h"

#include "common/ui/update_checker_impl.h"

#include <QThread>

namespace common {

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{
    thread_ = new QThread(this);
    impl_ = new UpdateCheckerImpl();

    impl_->moveToThread(thread_);

    connect(impl_, &UpdateCheckerImpl::finished, this, &UpdateChecker::finished);
    connect(impl_, &UpdateCheckerImpl::finished, impl_, &UpdateCheckerImpl::deleteLater);
    connect(impl_, &UpdateCheckerImpl::finished, thread_, &QThread::quit);

    connect(thread_, &QThread::started, impl_, &UpdateCheckerImpl::start);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);
}

UpdateChecker::~UpdateChecker()
{
    if (thread_ && thread_->isRunning())
    {
        thread_->quit();
        thread_->wait();
    }
}

void UpdateChecker::setUpdateServer(const QString& update_server)
{
    if (impl_)
        impl_->setUpdateServer(update_server);
}

void UpdateChecker::setPackageName(const QString& package_name)
{
    if (impl_)
        impl_->setPackageName(package_name);
}

void UpdateChecker::start()
{
    if (thread_)
        thread_->start(QThread::LowPriority);
}

} // namespace common
