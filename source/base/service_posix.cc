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

#include <QCoreApplication>
#include <QTimer>

#include <signal.h>

#include "base/logging.h"
#include "base/crypto/scoped_crypto_initializer.h"

namespace base {

namespace {

Service* g_self = nullptr;

//--------------------------------------------------------------------------------------------------
QString sigToString(int sig)
{
    switch (sig)
    {
        case SIGKILL:
            return "SIGKILL";
        case SIGTERM:
            return "SIGTERM";
        case SIGHUP:
            return "SIGHUP";
        case SIGQUIT:
            return "SIGQUIT";
        case SIGINT:
            return "SIGINT";
        case SIGSTOP:
            return "SIGSTOP";
        case SIGABRT:
            return "SIGABRT";
        default:
            return "unknown";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
Service::Service(const QString& name, QObject* parent)
    : name_(name)
{
    LOG(INFO) << "Ctor";
    g_self = this;
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
    g_self = nullptr;
}

//--------------------------------------------------------------------------------------------------
int Service::exec(Application& application)
{
    LOG(INFO) << "Begin";

    if (signal(SIGKILL, signalHandler) == SIG_ERR)
        LOG(ERROR) << "Unable to install signal handler for SIGKILL";

    if (signal(SIGTERM, signalHandler) == SIG_ERR)
        LOG(ERROR) << "Unable to install signal handler for SIGTERM";

    if (signal(SIGHUP, signalHandler) == SIG_ERR)
        LOG(ERROR) << "Unable to install signal handler for SIGHUP";

    if (signal(SIGQUIT, signalHandler) == SIG_ERR)
        LOG(ERROR) << "Unable to install signal handler for SIGQUIT";

    if (signal(SIGINT, signalHandler) == SIG_ERR)
        LOG(ERROR) << "Unable to install signal handler for SIGQUIT";

    if (signal(SIGSTOP, signalHandler) == SIG_ERR)
        LOG(ERROR) << "Unable to install signal handler for SIGSTOP";

    if (signal(SIGABRT, signalHandler) == SIG_ERR)
        LOG(ERROR) << "Unable to install signal handler for SIGABRT";

    std::unique_ptr<ScopedCryptoInitializer> crypto_initializer =
        std::make_unique<ScopedCryptoInitializer>();
    CHECK(crypto_initializer->isSucceeded());

    LOG(INFO) << "Cryptography initialized successfully";

    QTimer::singleShot(0, this, &Service::onStart);

    LOG(INFO) << "Run message loop";
    int ret = application.exec();

    LOG(INFO) << "End";
    return ret;
}

//--------------------------------------------------------------------------------------------------
void Service::stopHandlerImpl()
{
    QTimer::singleShot(0, this, [this]()
    {
        // A message loop termination command was received.
        onStop();
        QCoreApplication::quit();
    });
}

//--------------------------------------------------------------------------------------------------
// static
void Service::signalHandler(int sig)
{
    LOG(INFO) << "Signal received: " << sigToString(sig) << " (" << sig << ")";

    switch (sig)
    {
        case SIGTERM:
        case SIGINT:
        case SIGQUIT:
        {
            if (g_self)
                g_self->stopHandlerImpl();
        }
        break;

        default:
            break;
    }
}

} // namespace base
