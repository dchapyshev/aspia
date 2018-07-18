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

#ifndef _ASPIA_BASE__SERVICE_IMPL_H
#define _ASPIA_BASE__SERVICE_IMPL_H

#include <QString>

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

#if defined(Q_OS_WIN)
    virtual void sessionChange(quint32 event, quint32 session_id) = 0;
#endif // defined(Q_OS_WIN)

    virtual void createApplication(int argc, char* argv[]) = 0;
    virtual int executeApplication() = 0;

private:
    static ServiceImpl* instance_;

    QString name_;
    QString display_name_;
    QString description_;

    Q_DISABLE_COPY(ServiceImpl)
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_IMPL_H
