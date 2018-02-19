//
// PROJECT:         Aspia
// FILE:            host/sas_injector_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/sas_injector_main.h"

#include "base/logging.h"
#include "host/sas_injector.h"

namespace aspia {

void SasInjectorMain()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    SasInjector().ExecuteService();

    ShutdownLogging();
}

} // namespace aspia
