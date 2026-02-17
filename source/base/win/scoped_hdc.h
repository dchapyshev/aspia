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

#ifndef BASE_WIN_SCOPED_HDC_H
#define BASE_WIN_SCOPED_HDC_H

#include <QtGlobal>
#include <qt_windows.h>

namespace base {

// Like ScopedHandle but for HDC. Only use this on HDCs returned from GetDC.
class ScopedGetDC
{
public:
    ScopedGetDC() = default;

    explicit ScopedGetDC(HWND hwnd)
    {
        getDC(hwnd);
    }

    ~ScopedGetDC() { close(); }

    void close()
    {
        if (hdc_)
            ReleaseDC(hwnd_, hdc_);

        hdc_ = nullptr;
        hwnd_ = nullptr;
    }

    void getDC(HWND hwnd)
    {
        close();

        hwnd_ = hwnd;
        hdc_ = GetDC(hwnd);
    }

    bool isValid() const { return hdc_ != nullptr; }
    operator HDC() const { return hdc_; }

private:
    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;

    Q_DISABLE_COPY(ScopedGetDC)
};

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// CreateCompatibleDC, CreateDC and CreateIC.
class ScopedCreateDC
{
public:
    ScopedCreateDC() = default;

    explicit ScopedCreateDC(HDC h)
        : hdc_(h)
    {
        // Nothing
    }

    ~ScopedCreateDC() { close(); }

    HDC get() { return hdc_; }

    void reset(HDC h = nullptr)
    {
        close();
        hdc_ = h;
    }

    bool isValid() const { return hdc_ != nullptr; }

    operator HDC() { return hdc_; }

private:
    void close()
    {
        if (hdc_)
            DeleteDC(hdc_);
    }

    HDC hdc_ = nullptr;

    Q_DISABLE_COPY(ScopedCreateDC)
};

} // namespace base

#endif // BASE_WIN_SCOPED_HDC_H
