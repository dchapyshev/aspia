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

#include "base/command_line.h"
#include "base/meta_types.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "build/version.h"
#include "proto/meta_types.h"
#include "relay/service.h"
#include "relay/settings.h"

#if defined(OS_WIN)
#include "base/win/mini_dump_writer.h"
#include "relay/win/service_util.h"
#else
#include "base/crypto/scoped_crypto_initializer.h"
#include "base/message_loop/message_loop.h"
#include "relay/controller.h"
#endif

#include <iostream>

namespace {

//--------------------------------------------------------------------------------------------------
void createConfig()
{
    std::filesystem::path settings_file_path = relay::Settings::filePath();

    std::error_code error_code;
    if (std::filesystem::exists(settings_file_path, error_code))
    {
        std::cout << "Settings file already exists. Continuation is impossible." << std::endl;
        return;
    }

    // Save the configuration file.
    relay::Settings settings;
    settings.reset();
    settings.flush();

    std::cout << "Configuration successfully created." << std::endl;
}

//--------------------------------------------------------------------------------------------------
void showHelp()
{
    std::cout << "aspia_relay [switch]" << std::endl
        << "Available switches:" << std::endl
#if defined(OS_WIN)
        << '\t' << "--install" << '\t' << "Install service" << std::endl
        << '\t' << "--remove"  << '\t' << "Remove service"  << std::endl
        << '\t' << "--start"   << '\t' << "Start service"   << std::endl
        << '\t' << "--stop"    << '\t' << "Stop service"    << std::endl
#endif // defined(OS_WIN)
        << '\t' << "--create-config" << '\t' << "Creates a configuration" << std::endl
        << '\t' << "--help"    << '\t' << "Show help"       << std::endl;
}

} // namespace

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    base::installFailureHandler(L"aspia_relay");
    base::ScopedLogging logging;

    base::CommandLine::init(0, nullptr); // On Windows ignores arguments.
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING << " (arch: " << ARCH_CPU_STRING << ")";
    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();

    if (command_line->hasSwitch(u"install"))
    {
        relay::installService();
    }
    else if (command_line->hasSwitch(u"remove"))
    {
        relay::removeService();
    }
    else if (command_line->hasSwitch(u"start"))
    {
        relay::startService();
    }
    else if (command_line->hasSwitch(u"stop"))
    {
        relay::stopService();
    }
    else if (command_line->hasSwitch(u"create-config"))
    {
        createConfig();
    }
    else if (command_line->hasSwitch(u"help"))
    {
        showHelp();
    }
    else
    {
        base::registerMetaTypes();
        proto::registerMetaTypes();

        relay::Service().exec(argc, argv);
    }

    return 0;
}
#else
//--------------------------------------------------------------------------------------------------
int main(int argc, const char* const* argv)
{
    base::ScopedLogging logging;

    base::CommandLine::init(argc, argv);
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING << " (arch: " << ARCH_CPU_STRING << ")";
    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();

    std::unique_ptr<base::ScopedCryptoInitializer> crypto_initializer =
        std::make_unique<base::ScopedCryptoInitializer>();

    if (command_line->hasSwitch(u"create-config"))
    {
        createConfig();
    }
    else if (command_line->hasSwitch(u"help"))
    {
        showHelp();
    }
    else
    {
        base::registerMetaTypes();
        proto::registerMetaTypes();

        relay::Service().exec();
    }
}
#endif
