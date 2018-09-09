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

#ifndef ASPIA_BASE__SERVICE_IMPL_H_
#define ASPIA_BASE__SERVICE_IMPL_H_

#include <QString>

#include "base/macros_magic.h"
#include "build_config.h"

namespace aspia {

class ServiceEventHandler;

class ServiceImpl
{
public:
    ServiceImpl(const QString& name, const QString& display_name, const QString& description);
    virtual ~ServiceImpl() = default;

    static ServiceImpl* instance() { return instance_; }

    QString serviceName() const { return name_; }
    QString serviceDisplayName() const { return display_name_; }
    QString serviceDescription() const { return description_; }

    int exec(int argc, char* argv[]);

protected:
    friend class ServiceEventHandler;

    virtual void start() = 0;
    virtual void stop() = 0;

#if defined(OS_WIN)
    virtual void sessionChange(uint32_t event, uint32_t session_id) = 0;
#endif // defined(OS_WIN)

    virtual void createApplication(int argc, char* argv[]) = 0;
    virtual int executeApplication() = 0;

private:
    static ServiceImpl* instance_;

    QString name_;
    QString display_name_;
    QString description_;

    DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

} // namespace aspia

#endif // ASPIA_BASE__SERVICE_IMPL_H_
