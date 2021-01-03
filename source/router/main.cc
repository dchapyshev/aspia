//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/peer/user.h"
#include "router/database_factory_sqlite.h"
#include "router/database.h"
#include "router/settings.h"

#if defined(OS_WIN)
#include "router/win/service.h"
#include "router/win/service_util.h"
#else
#include "base/crypto/scoped_crypto_initializer.h"
#include "base/message_loop/message_loop.h"
#include "router/server.h"
#endif

#include <iostream>

namespace {

void initLogging()
{
    router::Settings settings;

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = settings.minLogLevel();

    base::initLogging(logging_settings);
}

void shutdownLogging()
{
    base::shutdownLogging();
}

bool generateKeys(base::ByteArray* private_key, base::ByteArray* public_key)
{
    base::KeyPair key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        std::cout << "Failed to generate keys" << std::endl;
        return false;
    }

    *private_key = key_pair.privateKey();
    *public_key = key_pair.publicKey();

    if (private_key->empty() || public_key->empty())
    {
        std::cout << "Empty keys generated";
        return false;
    }

    return true;
}

void generateAndPrintKeys()
{
    base::ByteArray private_key;
    base::ByteArray public_key;

    if (!generateKeys(&private_key, &public_key))
        return;

    std::cout << "Private key: " << base::toHex(private_key) << std::endl;
    std::cout << "Public key: " << base::toHex(public_key) << std::endl;
}

void createConfig()
{
    std::filesystem::path settings_file_path = router::Settings::filePath();

    std::error_code error_code;
    if (std::filesystem::exists(settings_file_path, error_code))
    {
        std::cout << "Settings file already exists. Continuation is impossible." << std::endl;
        return;
    }

    std::filesystem::path public_key_path;
    if (!base::BasePaths::commonAppData(&public_key_path))
    {
        std::cout << "Failed to get the path to the current directory" << std::endl;
        return;
    }

    public_key_path.append("aspia/router.pub");

    if (std::filesystem::exists(public_key_path, error_code))
    {
        std::cout << "Public key file already exists. Continuation is impossible." << std::endl;
        return;
    }

    std::unique_ptr<router::Database> db = router::DatabaseFactorySqlite().createDatabase();
    if (!db)
    {
        db = router::DatabaseFactorySqlite().openDatabase();
        if (db)
        {
            std::cout << "Database already exists. Continuation is impossible." << std::endl;
            return;
        }
    }

    const char16_t kUserName[] = u"admin";
    const char16_t kPassword[] = u"admin";

    base::User user = base::User::create(kUserName, kPassword);
    if (!user.isValid())
    {
        std::cout << "Failed to create user" << std::endl;
        return;
    }

    if (!db->addUser(user))
    {
        std::cout << "Failed to add user to database" << std::endl;
        return;
    }

    std::cout << "User name: " << base::local8BitFromUtf16(kUserName) << std::endl;
    std::cout << "Password: " << base::local8BitFromUtf16(kPassword) << std::endl;

    base::ByteArray private_key;
    base::ByteArray public_key;
    if (!generateKeys(&private_key, &public_key))
        return;

    if (!base::writeFile(public_key_path, base::toHex(public_key)))
    {
        std::cout << "Failed to write public key to file: " << public_key_path << std::endl;
        return;
    }
    else
    {
        std::cout << "Public key file: " << public_key_path << std::endl;
    }

    // Save the configuration file.
    router::Settings settings;
    settings.reset();
    settings.setPrivateKey(private_key);
    settings.flush();

    std::cout << "Configuration successfully created. Don't forget to change your password!"
              << std::endl;
}

void showHelp()
{
    std::cout << "aspia_router [switch]" << std::endl
        << "Available switches:" << std::endl
#if defined(OS_WIN)
        << '\t' << "--install" << '\t' << "Install service" << std::endl
        << '\t' << "--remove" << '\t' << "Remove service" << std::endl
        << '\t' << "--start" << '\t' << "Start service" << std::endl
        << '\t' << "--stop" << '\t' << "Stop service" << std::endl
#endif // defined(OS_WIN)
        << '\t' << "--create-config" << '\t' << "Creates a configuration" << std::endl
        << '\t' << "--keygen" << '\t' << "Generating public and private keys" << std::endl
        << '\t' << "--help" << '\t' << "Show help" << std::endl;
}

} // namespace

#if defined(OS_WIN)
int wmain()
{
    initLogging();

    base::CommandLine::init(0, nullptr); // On Windows ignores arguments.
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    if (command_line->hasSwitch(u"install"))
    {
        router::installService();
    }
    else if (command_line->hasSwitch(u"remove"))
    {
        router::removeService();
    }
    else if (command_line->hasSwitch(u"start"))
    {
        router::startService();
    }
    else if (command_line->hasSwitch(u"stop"))
    {
        router::stopService();
    }
    else if (command_line->hasSwitch(u"keygen"))
    {
        generateAndPrintKeys();
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
        router::Service().exec();
    }

    shutdownLogging();
    return 0;
}
#else
int main(int argc, const char* const* argv)
{
    initLogging();

    base::CommandLine::init(argc, argv);
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    std::unique_ptr<base::ScopedCryptoInitializer> crypto_initializer =
        std::make_unique<base::ScopedCryptoInitializer>();

    if (command_line->hasSwitch(u"keygen"))
    {
        generateAndPrintKeys();
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
        std::unique_ptr<base::MessageLoop> message_loop =
            std::make_unique<base::MessageLoop>(base::MessageLoop::Type::ASIO);

        std::unique_ptr<router::Server> server =
            std::make_unique<router::Server>(message_loop->taskRunner());

        server->start();
        message_loop->run();

        server.reset();
        message_loop.reset();
    }

    crypto_initializer.reset();
    shutdownLogging();
}
#endif
