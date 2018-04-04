//
// PROJECT:         Aspia
// FILE:            base/service_impl.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_IMPL_H
#define _ASPIA_BASE__SERVICE_IMPL_H

#include <QScopedPointer>
#include <QString>

namespace aspia {

class ServiceEventHandler;

class ServiceImpl
{
public:
    ServiceImpl(const QString& name);
    virtual ~ServiceImpl() = default;

    static ServiceImpl* instance() { return instance_; }
    QString serviceName() const { return name_; }
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
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_IMPL_H
