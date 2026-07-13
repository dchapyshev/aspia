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

#ifndef CLIENT_WIN_SCOPED_MMCSS_REGISTRATION_H
#define CLIENT_WIN_SCOPED_MMCSS_REGISTRATION_H

#include <QtClassHelperMacros>
#include <qt_windows.h>

class ScopedMMCSSRegistration
{
public:
    explicit ScopedMMCSSRegistration(const wchar_t* task_name);
    ~ScopedMMCSSRegistration();

    bool isSucceeded() const;

private:
    HANDLE mmcss_handle_ = nullptr;
    Q_DISABLE_COPY_MOVE(ScopedMMCSSRegistration)
};

#endif // CLIENT_WIN_SCOPED_MMCSS_REGISTRATION_H
