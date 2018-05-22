//
// PROJECT:         Aspia
// FILE:            base/service_impl_win.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/service_impl.h"

#if !defined(Q_OS_WIN)
#error This file for MS Windows only
#endif // defined(Q_OS_WIN)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QSemaphore>
#include <QThread>

#include <condition_variable>
#include <mutex>

#include "base/service_controller.h"
#include "base/system_error_code.h"

namespace aspia {

class ServiceHandler : public QThread
{
public:
    ServiceHandler();
    ~ServiceHandler();

    void setStatus(DWORD status);

    static ServiceHandler* instance;

    bool service_handler_started = true;
    bool running_in_console = false;

    QSemaphore create_app_start_semaphore;
    QSemaphore create_app_end_semaphore;

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

    Q_DISABLE_COPY(ServiceHandler)
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
    static void postSessionChangeEvent(quint32 event, quint32 session_id);

    class SessionChangeEvent : public QEvent
    {
    public:
        SessionChangeEvent(quint32 event, quint32 session_id)
            : QEvent(QEvent::Type(kSessionChangeEvent)),
              event_(event),
              session_id_(session_id)
        {
            // Nothing
        }

        quint32 event() const { return event_; }
        quint32 sessionId() const { return session_id_; }

    private:
        quint32 event_;
        quint32 session_id_;

        Q_DISABLE_COPY(SessionChangeEvent)
    };

protected:
    // QObject implementation.
    void customEvent(QEvent* event) override;

private:
    Q_DISABLE_COPY(ServiceEventHandler)
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
        qWarning() << "SetServiceStatus failed: " << lastSystemErrorString();
        return;
    }
}

void ServiceHandler::run()
{
    SERVICE_TABLE_ENTRYW service_table[2];

    service_table[0].lpServiceName = const_cast<wchar_t*>(
        reinterpret_cast<const wchar_t*>(ServiceImpl::instance()->serviceName().utf16()));
    service_table[0].lpServiceProc = ServiceHandler::serviceMain;
    service_table[1].lpServiceName = nullptr;
    service_table[1].lpServiceProc = nullptr;

    if (!StartServiceCtrlDispatcherW(service_table))
    {
        DWORD error_code = GetLastError();

        if (error_code == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
        {
            running_in_console = true;
        }
        else
        {
            qWarning() << "StartServiceCtrlDispatcherW failed: "
                       << systemErrorCodeToString(error_code);
        }

        service_handler_started = false;
        create_app_start_semaphore.release();
    }
}

// static
void WINAPI ServiceHandler::serviceMain(DWORD /* argc */, LPWSTR* /* argv */)
{
    if (!instance || !ServiceImpl::instance())
        return;

    // Start creating the QCoreApplication instance.
    instance->create_app_start_semaphore.release();

    // Waiting for the completion of the creation.
    instance->create_app_end_semaphore.acquire();

    instance->status_handle_ = RegisterServiceCtrlHandlerExW(
        reinterpret_cast<const wchar_t*>(ServiceImpl::instance()->serviceName().utf16()),
        serviceControlHandler,
        nullptr);

    if (!instance->status_handle_)
    {
        qWarning() << "RegisterServiceCtrlHandlerExW failed: " << lastSystemErrorString();
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
void ServiceEventHandler::postSessionChangeEvent(quint32 event, quint32 session_id)
{
    if (instance)
        QCoreApplication::postEvent(instance, new SessionChangeEvent(event, session_id));
}

void ServiceEventHandler::customEvent(QEvent* event)
{
    switch (event->type())
    {
        case ServiceEventHandler::kStartEvent:
            ServiceImpl::instance()->start();
            break;

        case ServiceEventHandler::kStopEvent:
            ServiceImpl::instance()->stop();
            QCoreApplication::instance()->quit();
            break;

        case ServiceEventHandler::kSessionChangeEvent:
        {
            SessionChangeEvent* session_change_event =
                reinterpret_cast<SessionChangeEvent*>(event);

            ServiceImpl::instance()->sessionChange(
                session_change_event->event(), session_change_event->sessionId());
        }
        break;

        default:
            return;
    }

    ServiceHandler::instance->event_lock.lock();

    // Set the event flag is processed.
    ServiceHandler::instance->event_processed = true;

    // Notify waiting thread for the end of processing.
    ServiceHandler::instance->event_condition.notify_all();

    ServiceHandler::instance->event_lock.unlock();
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

    // Starts handler thread.
    handler->start();

    // Waiting for the launch ServiceHandler::serviceMain.
    if (!handler->create_app_start_semaphore.tryAcquire(1, 20000))
    {
        qWarning("Function serviceMain was not called at the specified time interval");
        return 1;
    }

    if (!handler->service_handler_started && !handler->running_in_console)
        return 1;

    // Creates QCoreApplication.
    createApplication(argc, argv);

    QScopedPointer<QCoreApplication> application(QCoreApplication::instance());
    if (application.isNull())
    {
        qWarning("Application instance is null");
        return 1;
    }

    if (handler->running_in_console)
    {
        QCommandLineOption install_option(QStringLiteral("install"),
                                          QCoreApplication::tr("Install service"));
        QCommandLineOption remove_option(QStringLiteral("remove"),
                                         QCoreApplication::tr("Remove service"));
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
                controller.setDescription(description_);
                controller.start();
            }
        }
        else if (parser.isSet(remove_option))
        {
            ServiceController controller = ServiceController::open(name_);
            if (controller.isValid())
            {
                if (controller.isRunning())
                    controller.stop();
                controller.remove();
            }
        }

        return 0;
    }

    QScopedPointer<ServiceEventHandler> event_handler(new ServiceEventHandler());

    // Now we can complete the registration of the service.
    handler->create_app_end_semaphore.release();

    // Calls QCoreApplication::exec().
    int ret = executeApplication();

    // Set the state of the service and wait for the thread to complete.
    handler->setStatus(SERVICE_STOPPED);
    handler->wait();

    event_handler.reset();
    return ret;
}

} // namespace aspia
