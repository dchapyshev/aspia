//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__APPLICATION_H
#define CLIENT__UI__APPLICATION_H

#include "client/ui/client_settings.h"
#include "qt_base/application.h"

namespace client {

class Application : public qt_base::Application
{
    Q_OBJECT

public:
    Application(int& argc, char* argv[]);
    virtual ~Application() override = default;

    static Application* instance();

    ClientSettings& settings() { return settings_; }

private:
    ClientSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(Application);
};

} // namespace host

#endif // HOST__UI__APPLICATION_H
