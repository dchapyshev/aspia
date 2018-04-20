//
// PROJECT:         Aspia
// FILE:            base/service.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_H
#define _ASPIA_BASE__SERVICE_H

#include <QPointer>

#include "base/service_impl.h"

namespace aspia {

template <class Application>
class Service : public ServiceImpl
{
public:
    Service(const QString& name)
        : ServiceImpl(name)
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
        return application_->exec();
    }

private:
    QPointer<Application> application_;
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_H
