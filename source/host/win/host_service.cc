//
// PROJECT:         Aspia
// FILE:            host/host_service.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/win/host_service.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSettings>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>

#include "base/win/security_helpers.h"
#include "base/system_error_code.h"
#include "host/host_server.h"
#include "network/firewall_manager.h"
#include "version.h"

namespace aspia {

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

const char kFirewallRuleName[] = "Aspia Host Service";

} // namespace

HostService::HostService()
    : Service<QCoreApplication>(QStringLiteral("aspia-host-service"))
{
    // Nothing
}

void HostService::start()
{
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    QCoreApplication* app = application();

    app->setOrganizationName(QStringLiteral("Aspia"));
    app->setApplicationName(QStringLiteral("Host"));
    app->setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

    file_logger_.reset(new FileLogger());
    file_logger_->startLogging(*app);

    com_initializer_.reset(new ScopedCOMInitializer());
    if (!com_initializer_->isSucceeded())
    {
        qFatal("COM not initialized");
        app->quit();
        return;
    }

    initializeComSecurity(kComProcessSd, kComProcessMandatoryLabel, false);

    QSettings settings;
    int port = settings.value(QStringLiteral("TcpPort"), kDefaultHostTcpPort).toInt();

    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
    {
        firewall.addTCPRule(kFirewallRuleName,
                            QCoreApplication::tr("Allow incoming connections"),
                            port);
    }

    server_ = new HostServer();

    if (!server_->start(port))
    {
        delete server_;
        app->quit();
    }
}

void HostService::stop()
{
    delete server_;

    // Limiting the scope of the class.
    // After deinitializing the COM, the destructor will not be able to complete its work.
    {
        FirewallManager firewall(QCoreApplication::applicationFilePath());
        if (firewall.isValid())
            firewall.deleteRuleByName(kFirewallRuleName);
    }

    com_initializer_.reset();
}

void HostService::sessionChange(quint32 event, quint32 session_id)
{
    if (!server_.isNull())
        server_->setSessionChanged(event, session_id);
}

} // namespace aspia
