//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/service.h"

#include <QTimer>
#include <QThread>

#include "base/application.h"
#include "base/logging.h"
#include "base/crypto/scoped_crypto_initializer.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/security_helpers.h"

#include <sddl.h>

namespace base {

namespace {

// Concatenates ACE type, permissions and sid given as SDDL strings into an ACE
// definition in SDDL form.
#define SDDL_ACE(type, permissions, sid) L"(" type L";;" permissions L";;;" sid L")"

// Text representation of COM_RIGHTS_EXECUTE and COM_RIGHTS_EXECUTE_LOCAL
// permission bits that is used in the SDDL definition below.
#define SDDL_COM_EXECUTE_LOCAL L"0x3"

// Security descriptor allowing local processes running under SYSTEM or
// LocalService accounts to call COM methods exposed by the daemon.
const wchar_t kComProcessSd[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":"
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SYSTEM)
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SERVICE);

// Appended to |kComProcessSd| to specify that only callers running at medium
// or higher integrity level are allowed to call COM methods exposed by the
// daemon.
const wchar_t kComProcessMandatoryLabel[] =
    SDDL_SACL L":"
    SDDL_ACE(SDDL_MANDATORY_LABEL, SDDL_NO_EXECUTE_UP, SDDL_ML_MEDIUM);

//--------------------------------------------------------------------------------------------------
QString serviceStateToString(DWORD state)
{
    switch (state)
    {
        case SERVICE_CONTINUE_PENDING:
            return "SERVICE_CONTINUE_PENDING";
        case SERVICE_PAUSE_PENDING:
            return "SERVICE_PAUSE_PENDING";
        case SERVICE_PAUSED:
            return "SERVICE_PAUSED";
        case SERVICE_RUNNING:
            return "SERVICE_RUNNING";
        case SERVICE_START_PENDING:
            return "SERVICE_START_PENDING";
        case SERVICE_STOP_PENDING:
            return "SERVICE_STOP_PENDING";
        case SERVICE_STOPPED:
            return "SERVICE_STOPPED";
        default:
            return "Unknown State (" + QString::number(state) + ")";
    }
}

class ServiceThread final : public QThread
{
public:
    ServiceThread(Service* service);
    ~ServiceThread();

    using EventCallback = std::function<void()>;

    void setStatus(DWORD status);
    void doEvent(EventCallback callback, bool quit = false);

    static ServiceThread* self;

    enum class State
    {
        NOT_STARTED,
        ERROR_OCCURRED,
        SERVICE_MAIN_CALLED,
        MESSAGE_LOOP_CREATED,
        RUNNING_AS_CONSOLE,
        RUNNING_AS_SERVICE
    };

    std::condition_variable startup_condition;
    std::mutex startup_lock;
    State startup_state = State::NOT_STARTED;

    std::condition_variable event_condition;
    std::mutex event_lock;
    bool event_processed = false;

protected:
    // QThread implementation.
    void run() final;

private:
    static void WINAPI serviceMain(DWORD argc, LPWSTR* argv);
    static DWORD WINAPI serviceControlHandler(
        DWORD control_code, DWORD event_type, LPVOID event_data, LPVOID context);

    Service* service_;

    SERVICE_STATUS_HANDLE status_handle_ = nullptr;
    SERVICE_STATUS status_;

    Q_DISABLE_COPY(ServiceThread)
};

//================================================================================================
// ServiceThread implementation.
//================================================================================================

//--------------------------------------------------------------------------------------------------
ServiceThread* ServiceThread::self = nullptr;

//--------------------------------------------------------------------------------------------------
ServiceThread::ServiceThread(Service* service)
    : service_(service)
{
    LOG(INFO) << "Ctor";

    DCHECK(!self);
    self = this;

    memset(&status_, 0, sizeof(status_));
}

//--------------------------------------------------------------------------------------------------
ServiceThread::~ServiceThread()
{
    LOG(INFO) << "Dtor";

    setStatus(SERVICE_STOPPED);
    wait();

    DCHECK(self);
    self = nullptr;
}

//--------------------------------------------------------------------------------------------------
void ServiceThread::setStatus(DWORD status)
{
    LOG(INFO) << "Service status changed:" << serviceStateToString(status);

    status_.dwServiceType = SERVICE_WIN32;
    status_.dwControlsAccepted = 0;
    status_.dwCurrentState = status;
    status_.dwWin32ExitCode = NO_ERROR;
    status_.dwServiceSpecificExitCode = NO_ERROR;
    status_.dwWaitHint = 0;

    if (status == SERVICE_RUNNING)
    {
        status_.dwControlsAccepted =
            SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE |
            SERVICE_ACCEPT_POWEREVENT;
    }

    if (status != SERVICE_RUNNING && status != SERVICE_STOPPED)
        ++status_.dwCheckPoint;
    else
        status_.dwCheckPoint = 0;

    if (!SetServiceStatus(status_handle_, &status_))
    {
        PLOG(ERROR) << "SetServiceStatus failed";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceThread::doEvent(EventCallback callback, bool quit)
{
    std::unique_lock lock(event_lock);
    event_processed = false;

    QTimer::singleShot(0, QCoreApplication::instance(), [callback, quit]()
    {
        std::scoped_lock lock(self->event_lock);

        callback();

        if (quit)
        {
            // A message loop termination command was received.
            QCoreApplication::quit();
        }

        // Set the event flag is processed.
        self->event_processed = true;

        // Notify waiting thread for the end of processing.
        self->event_condition.notify_all();
    });

    // Wait for the event to be processed by the application.
    while (!event_processed)
        event_condition.wait(lock);
}

//--------------------------------------------------------------------------------------------------
void ServiceThread::run()
{
    SERVICE_TABLE_ENTRYW service_table[2];

    service_table[0].lpServiceName = const_cast<wchar_t*>(qUtf16Printable(service_->name()));
    service_table[0].lpServiceProc = ServiceThread::serviceMain;
    service_table[1].lpServiceName = nullptr;
    service_table[1].lpServiceProc = nullptr;

    if (!StartServiceCtrlDispatcherW(service_table))
    {
        std::scoped_lock lock(self->startup_lock);

        DWORD error_code = GetLastError();
        if (error_code == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
        {
            self->startup_state = State::RUNNING_AS_CONSOLE;
        }
        else
        {
            LOG(ERROR) << "StartServiceCtrlDispatcherW failed:" << SystemError::toString(error_code);
            self->startup_state = State::ERROR_OCCURRED;
        }

        self->startup_condition.notify_all();
    }
    else
    {
        PLOG(ERROR) << "StartServiceCtrlDispatcherW failed";
    }
}

//--------------------------------------------------------------------------------------------------
// static
void WINAPI ServiceThread::serviceMain(DWORD /* argc */, LPWSTR* /* argv */)
{
    if (!self)
        return;

    // Start creating the application instance.
    {
        std::scoped_lock lock(self->startup_lock);
        self->startup_state = State::SERVICE_MAIN_CALLED;
        self->startup_condition.notify_all();
    }

    // Waiting for the completion of the creation.
    {
        std::unique_lock lock(self->startup_lock);

        while (self->startup_state != State::MESSAGE_LOOP_CREATED)
            self->startup_condition.wait(lock);

        self->startup_state = State::RUNNING_AS_SERVICE;
    }

    self->status_handle_ = RegisterServiceCtrlHandlerExW(
        qUtf16Printable(self->service_->name()), serviceControlHandler, nullptr);

    if (!self->status_handle_)
    {
        PLOG(ERROR) << "RegisterServiceCtrlHandlerExW failed";
        return;
    }

    self->setStatus(SERVICE_START_PENDING);
    self->doEvent(std::bind(&Service::onStart, self->service_));
    self->setStatus(SERVICE_RUNNING);
}

//--------------------------------------------------------------------------------------------------
// static
DWORD WINAPI ServiceThread::serviceControlHandler(
    DWORD control_code, DWORD event_type, LPVOID event_data, LPVOID /* context */)
{
    switch (control_code)
    {
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        case SERVICE_CONTROL_SESSIONCHANGE:
        {
            if (!self)
                return NO_ERROR;

            SessionStatus session_status = static_cast<SessionStatus>(event_type);
            SessionId session_id =
                reinterpret_cast<WTSSESSION_NOTIFICATION*>(event_data)->dwSessionId;

            self->doEvent(std::bind(
                &Service::onSessionEvent, self->service_, session_status, session_id));
        }
        return NO_ERROR;

        case SERVICE_CONTROL_POWEREVENT:
        {
            if (!self)
                return NO_ERROR;

            self->doEvent(std::bind(&Service::onPowerEvent, self->service_, event_type));
        }
        return NO_ERROR;

        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
        {
            if (!self)
                return NO_ERROR;

            if (control_code == SERVICE_CONTROL_STOP)
                self->setStatus(SERVICE_STOP_PENDING);

            self->doEvent(std::bind(&Service::onStop, self->service_), true);
        }
        return NO_ERROR;

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
Service::Service(const QString& name, QObject* parent)
    : QObject(parent),
      name_(name)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
int Service::exec(Application& application)
{
    LOG(INFO) << "Begin";

    std::unique_ptr<ScopedCryptoInitializer> crypto_initializer =
        std::make_unique<ScopedCryptoInitializer>();
    CHECK(crypto_initializer->isSucceeded());

    std::unique_ptr<ScopedCOMInitializer> com_initializer =
        std::make_unique<ScopedCOMInitializer>();
    CHECK(com_initializer->isSucceeded());

    initializeComSecurity(kComProcessSd, kComProcessMandatoryLabel, false);

    std::unique_ptr<ServiceThread> service_thread = std::make_unique<ServiceThread>(this);

    // Waiting for the launch ServiceThread::serviceMain.
    {
        std::unique_lock lock(service_thread->startup_lock);
        service_thread->startup_state = ServiceThread::State::NOT_STARTED;

        // Starts handler thread.
        service_thread->start();

        while (service_thread->startup_state == ServiceThread::State::NOT_STARTED)
            service_thread->startup_condition.wait(lock);

        if (service_thread->startup_state == ServiceThread::State::ERROR_OCCURRED)
            return 1;
    }

    // Now we can complete the registration of the service.
    {
        std::scoped_lock lock(service_thread->startup_lock);
        service_thread->startup_state = ServiceThread::State::MESSAGE_LOOP_CREATED;
        service_thread->startup_condition.notify_all();
    }

    int ret = application.exec();
    service_thread.reset();

    LOG(INFO) << "End";
    return ret;
}

} // namespace base
