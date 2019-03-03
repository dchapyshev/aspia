//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "updater/update_checker.h"
#include "updater/update_checker_impl.h"

#include <QThread>

namespace updater {

Checker::Checker(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<UpdateInfo>();

    thread_ = new QThread(this);
    impl_ = new CheckerImpl();

    impl_->moveToThread(thread_);

    connect(impl_, &CheckerImpl::finished, this, &Checker::finished);
    connect(impl_, &CheckerImpl::finished, impl_, &CheckerImpl::deleteLater);
    connect(impl_, &CheckerImpl::finished, thread_, &QThread::quit);

    connect(thread_, &QThread::started, impl_, &CheckerImpl::start);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);
}

Checker::~Checker()
{
    if (thread_ && thread_->isRunning())
    {
        thread_->quit();
        thread_->wait();
    }
}

void Checker::setUpdateServer(const QString& update_server)
{
    if (impl_)
        impl_->setUpdateServer(update_server);
}

void Checker::setPackageName(const QString& package_name)
{
    if (impl_)
        impl_->setPackageName(package_name);
}

void Checker::start()
{
    if (thread_)
        thread_->start(QThread::LowPriority);
}

} // namespace updater
