//
// PROJECT:         Aspia
// FILE:            base/service_controller.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_CONTROLLER_H
#define _ASPIA_BASE__SERVICE_CONTROLLER_H

#include <QString>

#include "base/win/scoped_object.h"

namespace aspia {

class ServiceController
{
public:
    ServiceController();

    ServiceController(ServiceController&& other) noexcept;
    ServiceController& operator=(ServiceController&& other) noexcept;

    virtual ~ServiceController();

    static ServiceController open(const QString& name);
    static ServiceController install(const QString& name,
                                     const QString& display_name,
                                     const QString& file_path);
    static bool isInstalled(const QString& name);

    bool setDescription(const QString& description);
    QString description() const;

    QString filePath() const;

    bool isValid() const;
    bool isRunning() const;

    bool start();
    bool stop();
    bool remove();

protected:
    ServiceController(SC_HANDLE sc_manager, SC_HANDLE service);

private:
    ScopedScHandle sc_manager_;
    mutable ScopedScHandle service_;

    Q_DISABLE_COPY(ServiceController)
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_CONTROLLER_H
