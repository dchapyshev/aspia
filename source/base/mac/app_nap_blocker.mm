//
// SmartCafe Project
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

#include "base/mac/app_nap_blocker.h"

#include <QtGlobal>
#include <mutex>

#include <Cocoa/Cocoa.h>

namespace base {

namespace {

class AppNapBlocker
{
public:
    AppNapBlocker();
    ~AppNapBlocker();

    void addBlock();
    void releaseBlock();
    bool isBlocked() const;

private:
    void setBlocked(bool enable);

    mutable std::mutex id_lock_;
    id id_ = nullptr;
    uint32_t counter_ = 0;

    Q_DISABLE_COPY(AppNapBlocker)
};

//--------------------------------------------------------------------------------------------------
AppNapBlocker::AppNapBlocker() = default;

//--------------------------------------------------------------------------------------------------
AppNapBlocker::~AppNapBlocker()
{
    setBlocked(false);
}

//--------------------------------------------------------------------------------------------------
void AppNapBlocker::addBlock()
{
    std::scoped_lock lock(id_lock_);

    if (!counter_)
        setBlocked(true);

    ++counter_;
}

//--------------------------------------------------------------------------------------------------
void AppNapBlocker::releaseBlock()
{
    std::scoped_lock lock(id_lock_);

    if (!counter_)
        return;

    --counter_;

    if (!counter_)
        setBlocked(false);
}

//--------------------------------------------------------------------------------------------------
bool AppNapBlocker::isBlocked() const
{
    std::scoped_lock lock(id_lock_);
    return id_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
void AppNapBlocker::setBlocked(bool enable)
{
    if (enable == (id_ != nullptr))
        return;

    if (enable)
    {
        id_ = [[NSProcessInfo processInfo]
            beginActivityWithOptions: NSActivityUserInitiated
            reason: @"SmartCafe connection"];
        [id_ retain];
    }
    else
    {
        [[NSProcessInfo processInfo] endActivity:id_];
        [id_ release];
        id_ = nullptr;
    }
}

AppNapBlocker g_app_nap_blocker;

} // namespace

//--------------------------------------------------------------------------------------------------
void addAppNapBlock()
{
    g_app_nap_blocker.addBlock();
}

//--------------------------------------------------------------------------------------------------
void releaseAppNapBlock()
{
    g_app_nap_blocker.releaseBlock();
}

//--------------------------------------------------------------------------------------------------
bool isAppNapBlocked()
{
    return g_app_nap_blocker.isBlocked();
}

} // namespace base
