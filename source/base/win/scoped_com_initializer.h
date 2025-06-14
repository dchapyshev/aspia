//
// Aspia Project
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

#ifndef BASE_WIN_SCOPED_COM_INITIALIZER_H
#define BASE_WIN_SCOPED_COM_INITIALIZER_H

#include <QtGlobal>
#include <objbase.h>

namespace base {

// Initializes COM in the constructor (STA or MTA), and uninitializes COM in the
// destructor.
//
// WARNING: This should only be used once per thread, ideally scoped to a
// similar lifetime as the thread itself.  You should not be using this in
// random utility functions that make COM calls -- instead ensure these
// functions are running on a COM-supporting thread!
class ScopedCOMInitializer
{
public:
    // Enum value provided to initialize the thread as an MTA instead of STA.
    enum SelectMTA { kMTA };

    // Constructor for STA initialization.
    ScopedCOMInitializer();

    // Constructor for MTA initialization.
    explicit ScopedCOMInitializer(SelectMTA mta);

    ~ScopedCOMInitializer();

    bool isSucceeded() const;

private:
    void initialize(COINIT init);

    HRESULT hr_;

    Q_DISABLE_COPY(ScopedCOMInitializer)
};

} // namespace base

#endif // BASE_WIN_SCOPED_COM_INITIALIZER_H
