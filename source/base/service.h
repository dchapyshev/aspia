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

#ifndef BASE__SERVICE_H
#define BASE__SERVICE_H

#include "base/service_impl.h"

namespace base {

template <class Application>
class Service : public ServiceImpl
{
public:
    Service(const QString& name, const QString& display_name, const QString& description)
        : ServiceImpl(name, display_name, description)
    {
        // Nothing
    }

    virtual ~Service() = default;

protected:
    Application* application() const { return application_; }

    // ServiceImpl implementation.
    void createApplication(int argc, char* argv[]) override
    {
        application_ = new Application(argc, argv);
    }

    // ServiceImpl implementation.
    int executeApplication() override
    {
        return Application::exec();
    }

private:
    Application* application_ = nullptr;
};

} // namespace base

#endif // BASE__SERVICE_H
