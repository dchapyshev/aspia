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

#ifndef BASE_WIN_SCOPED_IMPERSONATOR_H
#define BASE_WIN_SCOPED_IMPERSONATOR_H

#include <QtGlobal>
#include <qt_windows.h>

namespace base {

class ScopedImpersonator
{
public:
    ScopedImpersonator();
    ~ScopedImpersonator();

    bool loggedOnUser(HANDLE user_token);
    bool anonymous();
    bool namedPipeClient(HANDLE named_pipe);

    void revertToSelf();

    bool isImpersonated() const { return impersonated_; }

private:
    bool impersonated_ = false;

    Q_DISABLE_COPY(ScopedImpersonator)
};

} // namespace base

#endif // BASE_WIN_SCOPED_IMPERSONATOR_H
