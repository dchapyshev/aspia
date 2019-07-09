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

#ifndef HOST__HOST_APPLICATION_H
#define HOST__HOST_APPLICATION_H

#include "host/host_settings.h"
#include "qt_base/application.h"

namespace host {

class Application : public qt_base::Application
{
    Q_OBJECT

public:
    Application(int& argc, char* argv[]);
    virtual ~Application() = default;

    static Application* instance();

    Settings& settings() { return settings_; }

public slots:
    void activate();

signals:
    void activated();

private:
    Settings settings_;

    DISALLOW_COPY_AND_ASSIGN(Application);
};

} // namespace host

#endif // HOST__HOST_APPLICATION_H
