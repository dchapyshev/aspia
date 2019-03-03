//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__HOST_SINGLE_APPLICATION_H
#define HOST__HOST_SINGLE_APPLICATION_H

#include "base/single_application.h"

namespace host {

class SingleApplication : public base::SingleApplication
{
    Q_OBJECT

public:
    SingleApplication(int& argc, char* argv[]);
    virtual ~SingleApplication() = default;

public slots:
    void activate();

signals:
    void activated();

private:
    DISALLOW_COPY_AND_ASSIGN(SingleApplication);
};

} // namespace host

#endif // HOST__HOST_SINGLE_APPLICATION_H
