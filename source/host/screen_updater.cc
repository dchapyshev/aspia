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

#include "host/screen_updater.h"
#include "host/screen_updater_impl.h"

namespace host {

//================================================================================================
// ScreenUpdater implementation.
//================================================================================================

ScreenUpdater::ScreenUpdater(Delegate* delegate, QObject* parent)
    : QObject(parent),
      delegate_(delegate)
{
    // Nothing
}

bool ScreenUpdater::start(const proto::desktop::Config& config)
{
    impl_ = new ScreenUpdaterImpl(this);
    return impl_->startUpdater(config);
}

void ScreenUpdater::selectScreen(int64_t screen_id)
{
    impl_->selectScreen(screen_id);
}

void ScreenUpdater::customEvent(QEvent* event)
{
    if (event->type() != ScreenUpdaterImpl::MessageEvent::kType)
        return;

    delegate_->onScreenUpdate(static_cast<ScreenUpdaterImpl::MessageEvent*>(event)->buffer());
}

} // namespace host
