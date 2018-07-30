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

#include "base/service_impl.h"

#if !defined(Q_OS_WIN)
#error This file for MS Windows only
#endif // defined(Q_OS_WIN)

#include <qt_windows.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QThread>

#include <condition_variable>
#include <mutex>

#include "base/errno_logging.h"
#include "base/service_controller.h"

namespace aspia {

class ServiceHandler : public QThread
{
public:
    ServiceHandler();
    ~ServiceHandler();

    void setStatus(DWORD status);

    static ServiceHandler* instance;

    enum StartupState
    {
        NotStarted,
        ErrorOccurred,
        ServiceMainCalled,
        ApplicationCreated,
        RunningAsConsole,
        RunningAsService
    };

    std::condition_variable startup_condition;
    std::mutex startup_lock;
    StartupState startup_state = NotStarted;

    std::condition_variable event_condition;
    std::mutex event_lock;
    bool event_processed = false;

protected:
    // QThread implementation.
    void run() override;

private:
    static void WINAPI serviceMain(DWORD argc, LPWSTR* argv);
    static DWORD WINAPI serviceControlHandler(
        DWORD control_code, DWORD event_type, LPVOID event_data, LPVOID context);

    SERVICE_STATUS_HANDLE status_handle_ = nullptr;
    SERVICE_STATUS status_;

    DISALLOW_COPY_AND_ASSIGN(ServiceHandler);
};

class ServiceEventHandler : public QObject
{
public:
    ServiceEventHandler();
    ~ServiceEventHandler();

    static ServiceEventHandler* instance;

    static const int kStartEvent = QEvent::User + 1;
    static const int kStopEvent = QEvent::User + 2;
    static const int kSessionChangeEvent = QEvent::User + 3;

    static void postStartEvent();
    static void postStopEvent();
    static void postSessionChangeEvent(uint32_t event, uint32_t session_id);

    class SessionChangeEvent : public QEvent
    {
    public:
        SessionChangeEvent(uint32_t event, uint32_t session_id)
            : QEvent(QEvent::Type(kSessionChangeEvent)),
              event_(event),
              session_id_(session_id)
        {
            // Nothing
        }

        uint32_t event() const { return event_; }
        uint32_t sessionId() const { return session_id_; }

    private:
        uint32_t event_;
        uint32_t session_id_;

        DISALLOW_COPY_AND_ASSIGN(SessionChangeEvent);
    };

protected:
    // QObject implementation.
    void customEvent(QEvent* event) override;

private:
    DISALLOW_COPY_AND_ASSIGN(ServiceEventHandler);
};

//================================================================================================
// ServiceHandler implementation.
//================================================================================================

ServiceHandler* ServiceHandler::instance = nullptr;

ServiceHandler::ServiceHandler()
{
    Q_ASSERT(!instance);
    instance = this;

    memset(&status_, 0, sizeof(status_));
}

ServiceHandler::~ServiceHandler()
{
    Q_ASSERT(instance);
    instance = nullptr;
}

void ServiceHandler::setStatus(DWORD status)
{
    status_.dwServiceType             = SERVICE_WIN32;
    status_.dwControlsAccepted        = 0;
    status_.dwCurrentState            = status;
    status_.dwWin32ExitCode           = NO_ERROR;
    status_.dwServiceSpecificExitCode = NO_ERROR;
    status_.dwWaitHint                = 0;

    if (status == SERVICE_RUNNING)
    {
        status_.dwControlsAccepted =
            SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE;
    }

    if (status != SERVICE_RUNNING && status != SERVICE_STOPPED)
        ++status_.dwCheckPoint;
    else
        status_.dwCheckPoint = 0;

    if (!SetServiceStatus(status_handle_, &status_))
    {
        qWarningErrno("SetServiceStatus failed");
        return;
    }
}

void ServiceHandler::run()
{
    SERVICE_TABLE_ENTRYW service_table[2];

    service_table[0].lpServiceName = const_cast<wchar_t*>(
        qUtf16Printable(ServiceImpl::instance()->serviceName()));
    service_table[0].lpServiceProc = ServiceHandler::serviceMain;
    service_table[1].lpServiceName = nullptr;
    service_table[1].lpServiceProc = nullptr;

    if (!StartServiceCtrlDispatcherW(service_table))
    {
        std::scoped_lock<std::mutex> lock(instance->startup_lock);

        DWORD error_code = GetLastError();
        if (error_code == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
        {
            instance->startup_state = RunningAsConsole;
        }
        else
        {
            qWarning() << "StartServiceCtrlDispatcherW failed: "
                       << errnoToString(error_code);
            instance->startup_state = ErrorOccurred;
        }

        instance->startup_condition.notify_all();
    }
}

// static
void WINAPI ServiceHandler::serviceMain(DWORD /* argc */, LPWSTR* /* argv */)
{
    if (!instance || !ServiceImpl::instance())
        return;

    // Start creating the QCoreApplication instance.
    {
        std::scoped_lock<std::mutex> lock(instance->startup_lock);
        instance->startup_state = ServiceMainCalled;
        instance->startup_condition.notify_all();
    }

    // Waiting for the completion of the creation.
    {
        std::unique_lock<std::mutex> lock(instance->startup_lock);

        while (instance->startup_state != ApplicationCreated)
            instance->startup_condition.wait(lock);

        instance->startup_state = RunningAsService;
    }

    instance->status_handle_ = RegisterServiceCtrlHandlerExW(
        qUtf16Printable(ServiceImpl::instance()->serviceName()),
        serviceControlHandler,
        nullptr);

    if (!instance->status_handle_)
    {
        qWarningErrno("RegisterServiceCtrlHandlerExW failed");
        return;
    }

    instance->setStatus(SERVICE_START_PENDING);

    std::unique_lock<std::mutex> lock(instance->event_lock);
    instance->event_processed = false;

    ServiceEventHandler::postStartEvent();

    // Wait for the event to be processed by the application.
    while (!instance->event_processed)
        instance->event_condition.wait(lock);

    instance->setStatus(SERVICE_RUNNING);
}

// static
DWORD WINAPI ServiceHandler::serviceControlHandler(
    DWORD control_code, DWORD event_type, LPVOID event_data, LPVOID /* context */)
{
    switch (control_code)
    {
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        case SERVICE_CONTROL_SESSIONCHANGE:
        {
            if (!instance)
                return NO_ERROR;

            std::unique_lock<std::mutex> lock(instance->event_lock);
            instance->event_processed = false;

            // Post event to application.
            ServiceEventHandler::postSessionChangeEvent(
                event_type, reinterpret_cast<WTSSESSION_NOTIFICATION*>(event_data)->dwSessionId);

            // Wait for the event to be processed by the application.
            while (!instance->event_processed)
                instance->event_condition.wait(lock);
        }
        return NO_ERROR;

        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
        {
            if (!instance)
                return NO_ERROR;

            if (control_code == SERVICE_CONTROL_STOP)
                instance->setStatus(SERVICE_STOP_PENDING);

            std::unique_lock<std::mutex> lock(instance->event_lock);
            instance->event_processed = false;

            // Post event to application.
            ServiceEventHandler::postStopEvent();

            // Wait for the event to be processed by the application.
            while (!instance->event_processed)
                instance->event_condition.wait(lock);
        }
        return NO_ERROR;

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

//================================================================================================
// ServiceEventHandler implementation.
//================================================================================================

ServiceEventHandler* ServiceEventHandler::instance = nullptr;

ServiceEventHandler::ServiceEventHandler()
{
    Q_ASSERT(!instance);
    instance = this;
}

ServiceEventHandler::~ServiceEventHandler()
{
    Q_ASSERT(instance);
    instance = nullptr;
}

// static
void ServiceEventHandler::postStartEvent()
{
    if (instance)
        QCoreApplication::postEvent(instance, new QEvent(QEvent::Type(kStartEvent)));
}

// static
void ServiceEventHandler::postStopEvent()
{
    if (instance)
        QCoreApplication::postEvent(instance, new QEvent(QEvent::Type(kStopEvent)));
}

// static
void ServiceEventHandler::postSessionChangeEvent(uint32_t event, uint32_t session_id)
{
    if (instance)
        QCoreApplication::postEvent(instance, new SessionChangeEvent(event, session_id));
}

void ServiceEventHandler::customEvent(QEvent* event)
{
    switch (event->type())
    {
        case kStartEvent:
            ServiceImpl::instance()->start();
            break;

        case kStopEvent:
            ServiceImpl::instance()->stop();
            QCoreApplication::instance()->quit();
            break;

        case kSessionChangeEvent:
        {
            SessionChangeEvent* session_change_event = dynamic_cast<SessionChangeEvent*>(event);
            if (!session_change_event)
                return;

            ServiceImpl::instance()->sessionChange(session_change_event->event(),
                                                   session_change_event->sessionId());
        }
        break;

        default:
            return;
    }

    std::scoped_lock<std::mutex> lock(ServiceHandler::instance->event_lock);

    // Set the event flag is processed.
    ServiceHandler::instance->event_processed = true;

    // Notify waiting thread for the end of processing.
    ServiceHandler::instance->event_condition.notify_all();
}

//================================================================================================
// ServiceImpl implementation.
//================================================================================================

ServiceImpl* ServiceImpl::instance_ = nullptr;

ServiceImpl::ServiceImpl(const QString& name,
                         const QString& display_name,
                         const QString& description)
    : name_(name),
      display_name_(display_name),
      description_(description)
{
    instance_ = this;
}

int ServiceImpl::exec(int argc, char* argv[])
{
    QScopedPointer<ServiceHandler> handler(new ServiceHandler());

    // Waiting for the launch ServiceHandler::serviceMain.
    {
        std::unique_lock<std::mutex> lock(handler->startup_lock);
        handler->startup_state = ServiceHandler::NotStarted;

        // Starts handler thread.
        handler->start();

        while (handler->startup_state == ServiceHandler::NotStarted)
            handler->startup_condition.wait(lock);

        if (handler->startup_state == ServiceHandler::ErrorOccurred)
            return 1;
    }

    // Creates QCoreApplication.
    createApplication(argc, argv);

    QScopedPointer<QCoreApplication> application(QCoreApplication::instance());
    if (application.isNull())
    {
        qWarning("Application instance is null");
        return 1;
    }

    if (handler->startup_state == ServiceHandler::RunningAsConsole)
    {
        QCommandLineOption install_option(QStringLiteral("install"),
                                          QStringLiteral("Install service"));
        QCommandLineOption remove_option(QStringLiteral("remove"),
                                         QStringLiteral("Remove service"));
        QCommandLineParser parser;

        parser.addHelpOption();
        parser.addOption(install_option);
        parser.addOption(remove_option);

        parser.process(application->arguments());

        if (parser.isSet(install_option))
        {
            ServiceController controller = ServiceController::install(
                name_, display_name_, application->applicationFilePath());
            if (controller.isValid())
            {
                printf("Service has been successfully installed. Starting...\n");
                controller.setDescription(description_);
                if (!controller.start())
                    printf("Service could not be started.");
                else
                    printf("Done.\n");
            }
        }
        else if (parser.isSet(remove_option))
        {
            ServiceController controller = ServiceController::open(name_);
            if (controller.isValid())
            {
                if (controller.isRunning())
                {
                    printf("Service is started. Stopping...\n");
                    if (!controller.stop())
                        printf("Error: Service could not be stopped.\n");
                    else
                        printf("Done.\n");
                }

                printf("Remove the service...\n");
                if (!controller.remove())
                    printf("Error: Service could not be removed.\n");
                else
                    printf("Done.\n");
            }
            else
            {
                printf("Could not access the service.\n"
                       "The service is not installed or you do not have administrator rights.\n");
            }
        }

        return 0;
    }

    QScopedPointer<ServiceEventHandler> event_handler(new ServiceEventHandler());

    // Now we can complete the registration of the service.
    {
        std::scoped_lock<std::mutex> lock(handler->startup_lock);
        handler->startup_state = ServiceHandler::ApplicationCreated;
        handler->startup_condition.notify_all();
    }

    // Calls QCoreApplication::exec().
    int ret = executeApplication();

    // Set the state of the service and wait for the thread to complete.
    handler->setStatus(SERVICE_STOPPED);

    if (handler->isRunning())
        handler->wait();

    event_handler.reset();
    handler.reset();
    application.reset();

    return ret;
}

} // namespace aspia
