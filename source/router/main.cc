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
#include "build/version.h"
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
    LOG(LS_INFO) << "Creation of initial configuration started";

    std::filesystem::path settings_file_path = router::Settings::filePath();

    LOG(LS_INFO) << "Settings file path: " << settings_file_path;

    std::error_code error_code;
    if (std::filesystem::exists(settings_file_path, error_code))
    {
        const char kMessage[] = "Settings file already exists. Continuation is impossible.";
        LOG(LS_WARNING) << kMessage;
        std::cout << kMessage << std::endl;
        return;
    }

    std::filesystem::path public_key_dir;
    if (!base::BasePaths::commonAppData(&public_key_dir))
    {
        LOG(LS_WARNING) << "base::BasePaths::commonAppData failed";
        std::cout << "Failed to get the path to the current directory" << std::endl;
        return;
    }

    public_key_dir.append("aspia");

    LOG(LS_INFO) << "Public key directory: " << public_key_dir;

    if (!std::filesystem::exists(public_key_dir, error_code))
    {
        LOG(LS_INFO) << "Public key directory does not exist ("
                     << base::utf16FromLocal8Bit(error_code.message()) << "). Attempt to create...";

        if (!std::filesystem::create_directories(public_key_dir, error_code))
        {
            LOG(LS_WARNING) << "Failed to create directory for public key: "
                            << base::utf16FromLocal8Bit(error_code.message());
            std::cout << "Failed to create directory for public key" << std::endl;
            return;
        }
        else
        {
            LOG(LS_INFO) << "Public key directory created successfully";
        }
    }
    else
    {
        LOG(LS_INFO) << "Public key directory already exists";
    }

    std::filesystem::path public_key_file = public_key_dir;
    public_key_file.append("router.pub");

    LOG(LS_INFO) << "Public key file: " << public_key_file;

    if (std::filesystem::exists(public_key_file, error_code))
    {
        const char kMessage[] = "Public key file already exists. Continuation is impossible.";
        LOG(LS_WARNING) << kMessage;
        std::cout << kMessage << std::endl;
        return;
    }
    else
    {
        LOG(LS_INFO) << "Public key does not exist yet";
    }

    std::unique_ptr<router::Database> db = router::DatabaseFactorySqlite().createDatabase();
    if (!db)
    {
        db = router::DatabaseFactorySqlite().openDatabase();
        if (db)
        {
            const char kMessage[] = "Database already exists. Continuation is impossible.";
            LOG(LS_WARNING) << kMessage;
            std::cout << kMessage << std::endl;
        }
        else
        {
            const char kMessage[] = "Failed to create new database.";
            LOG(LS_WARNING) << kMessage;
            std::cout << kMessage << std::endl;
        }
        return;
    }

    LOG(LS_INFO) << "Creating a user...";

    const char16_t kUserName[] = u"admin";
    const char16_t kPassword[] = u"admin";

    base::User user = base::User::create(kUserName, kPassword);
    if (!user.isValid())
    {
        const char kMessage[] = "Failed to create user";
        LOG(LS_WARNING) << kMessage;
        std::cout << kMessage << std::endl;
        return;
    }

    LOG(LS_INFO) << "User has been created. Adding a user to the database...";

    user.sessions = proto::ROUTER_SESSION_ADMIN | proto::ROUTER_SESSION_CLIENT;
    user.flags = base::User::ENABLED;

    if (!db->addUser(user))
    {
        const char kMessage[] = "Failed to add user to database";
        LOG(LS_WARNING) << kMessage;
        std::cout << kMessage << std::endl;
        return;
    }

    LOG(LS_INFO) << "User was successfully added to the database";

    std::cout << "User name: " << base::local8BitFromUtf16(kUserName) << std::endl;
    std::cout << "Password: " << base::local8BitFromUtf16(kPassword) << std::endl;

    LOG(LS_INFO) << "Generating encryption keys...";

    base::ByteArray private_key;
    base::ByteArray public_key;
    if (!generateKeys(&private_key, &public_key))
    {
        LOG(LS_WARNING) << "Failed to generate private and public keys";
        return;
    }

    LOG(LS_INFO) << "Private and public keys have been successfully generated";
    LOG(LS_INFO) << "Writing a public key to a file...";

    if (!base::writeFile(public_key_file, base::toHex(public_key)))
    {
        const char kMessage[] = "Failed to write public key to file: ";
        LOG(LS_WARNING) << kMessage << public_key_file;
        std::cout << kMessage << public_key_file << std::endl;
        return;
    }
    else
    {
        LOG(LS_INFO) << "Public key was successfully written to the file";
        std::cout << "Public key file: " << public_key_file << std::endl;
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

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();

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

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();

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
