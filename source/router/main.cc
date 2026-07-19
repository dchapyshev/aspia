//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QSysInfo>
#include <QStandardPaths>

#include "base/logging.h"
#include "base/service_controller.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/peer/router_user.h"
#include "base/threading/asio_event_dispatcher.h"
#include "build/version.h"
#include "router/database.h"
#include "router/service.h"
#include "router/settings.h"
#include "router/workers/relay_worker.h"
#include "router/workers/stun_worker.h"

namespace {

//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    std::unique_ptr<ServiceController> controller = ServiceController::open(Service::kName);
    if (!controller)
    {
        out << "Failed to access the service. Not enough rights or service not installed." << Qt::endl;
        return 1;
    }

    if (!controller->start())
    {
        out << "Failed to start the service." << Qt::endl;
        return 1;
    }

    out << "The service started successfully." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int stopService(QTextStream& out)
{
    std::unique_ptr<ServiceController> controller = ServiceController::open(Service::kName);
    if (!controller)
    {
        out << "Failed to access the service. Not enough rights or service not installed." << Qt::endl;
        return 1;
    }

    if (!controller->stop())
    {
        out << "Failed to stop the service." << Qt::endl;
        return 1;
    }

    out << "The service has stopped successfully." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int installService(QTextStream& out)
{
    // The service is never installed without a configuration. On a clean system there is none yet,
    // so this is a no-op (the package install must not fail); the user creates the configuration
    // first and runs --install afterwards. On an upgrade the existing configuration is present, so
    // the service is reinstalled and its parameters refreshed.
    Settings settings;
    if (settings.isEmpty())
    {
        out << "Configuration does not exist; the service was not installed. Create it with "
               "--create-config, then run --install." << Qt::endl;
        return 0;
    }

    std::unique_ptr<ServiceController> controller = ServiceController::install(
        Service::kName, Service::kDisplayName, QCoreApplication::applicationFilePath());
    if (!controller)
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller->setDescription(Service::kDescription);

#if defined(Q_OS_WINDOWS)
    // Start only after the network stack is up.
    controller->setDependencies({ "RpcSs", "Tcpip", "NDIS", "AFD" });
#endif

    // Run the service under its low-privilege account with access only to the directories it needs
    // (config files, database).
    const QString account = ServiceController::lowPrivilegeAccount(Service::kName);
    if (!account.isEmpty() &&
        !controller->setAccount(account, QString(), { BasePaths::appConfigDir(), BasePaths::appDataDir() }))
    {
        out << "Warning: failed to reduce service privileges. The service runs under the default "
               "system account." << Qt::endl;
    }

    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (ServiceController::isRunning(Service::kName))
        stopService(out);

    if (!ServiceController::remove(Service::kName))
    {
        out << "Failed to remove the service." << Qt::endl;
        return 1;
    }

    out << "The service was successfully deleted." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
bool generateKeys(SecureByteArray* private_key, QByteArray* public_key)
{
    KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        std::cout << "Failed to generate keys" << std::endl;
        return false;
    }

    *private_key = key_pair.privateKey();
    *public_key = key_pair.publicKey();

    if (private_key->isEmpty() || public_key->isEmpty())
    {
        std::cout << "Empty keys generated";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
int generateAndPrintKeys(QTextStream& out)
{
    SecureByteArray private_key;
    QByteArray public_key;

    if (!generateKeys(&private_key, &public_key))
        return 1;

    out << "Private key: " << private_key.toByteArray().toHex() << Qt::endl;
    out << "Public key: " << public_key.toHex() << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
QString publicKeyDirectory()
{
    return BasePaths::appConfigDir();
}

//--------------------------------------------------------------------------------------------------
int createConfig(QTextStream& out)
{
    out << "Creation of initial configuration started." << Qt::endl;

    Settings settings;
    QString settings_file_path = settings.filePath();

    out << "Settings file path: " << settings_file_path << Qt::endl;

    if (!settings.isEmpty())
    {
        out << "Settings file already exists. Continuation is impossible." << Qt::endl;
        return 1;
    }

    out << "Settings file does not exist yet." << Qt::endl;

    QString public_key_dir = publicKeyDirectory();
    if (public_key_dir.isEmpty())
    {
        out << "Failed to get the path to the config directory." << Qt::endl;
        return 1;
    }

    out << "Public key directory path: " << public_key_dir << Qt::endl;

    if (!QFileInfo::exists(public_key_dir))
    {
        out << "Public key directory does not exist. Attempt to create..." << Qt::endl;

        if (!QDir().mkpath(public_key_dir))
        {
            out << "Failed to create directory for public key" << Qt::endl;
            return 1;
        }
        else
        {
            out << "Public key directory created successfully." << Qt::endl;
        }
    }
    else
    {
        out << "Public key directory already exists." << Qt::endl;
    }

    QString host_public_key_file = public_key_dir + "/host.pub";
    QString relay_public_key_file = public_key_dir + "/relay.pub";

    out << "Host public key file: " << host_public_key_file << Qt::endl;
    out << "Relay public key file: " << relay_public_key_file << Qt::endl;

    if (QFileInfo::exists(host_public_key_file))
    {
        out << "Host public key file already exists. Continuation is impossible." << Qt::endl;
        return 1;
    }

    if (QFileInfo::exists(relay_public_key_file))
    {
        out << "Relay public key file already exists. Continuation is impossible." << Qt::endl;
        return 1;
    }

    if (QFileInfo::exists(Database::filePath()))
    {
        out << "Database already exists. Continuation is impossible." << Qt::endl;
        return 1;
    }

    Database& db = Database::instance();
    if (!db.isValid())
    {
        out << "Failed to create new database." << Qt::endl;
        return 1;
    }

    out << "Creating a user..." << Qt::endl;

    const char kUserName[] = "admin";
    const char kPassword[] = "admin";

    RouterUser user = RouterUser::create(kUserName, SecureString(kPassword));
    if (!user.isValid())
    {
        out << "Failed to create user." << Qt::endl;
        return 1;
    }

    out << "User has been created. Adding a user to the database..." << Qt::endl;

    user.sessions = proto::router::SESSION_TYPE_ADMIN | proto::router::SESSION_TYPE_MANAGER |
                    proto::router::SESSION_TYPE_CLIENT;
    user.flags = User::ENABLED;

    if (!db.addUser(user))
    {
        out << "Failed to add user to database." << Qt::endl;
        return 1;
    }

    out << "User was successfully added to the database." << Qt::endl;
    out << "Generating encryption keys..." << Qt::endl;

    SecureByteArray host_private_key;
    QByteArray host_public_key;
    if (!generateKeys(&host_private_key, &host_public_key))
        return 1;

    SecureByteArray relay_private_key;
    QByteArray relay_public_key;
    if (!generateKeys(&relay_private_key, &relay_public_key))
        return 1;

    out << "Private and public keys have been successfully generated." << Qt::endl;
    out << "Writing public keys to files..." << Qt::endl;

    if (!writeFile(host_public_key_file, host_public_key.toHex()))
    {
        out << "Failed to write public key to file: " << host_public_key_file << Qt::endl;
        return 1;
    }

    if (!writeFile(relay_public_key_file, relay_public_key.toHex()))
    {
        out << "Failed to write public key to file: " << relay_public_key_file << Qt::endl;
        return 1;
    }

    out << "Generate seed key..." << Qt::endl;
    QByteArray seed_key = Random::byteArray(64);
    if (seed_key.isEmpty())
    {
        out << "Unable to generate seed key" << Qt::endl;
        return 1;
    }

    out << "Seed key successfully generated" << Qt::endl;

    // Save the configuration file.
    settings.reset();
    settings.setHostPrivateKey(host_private_key);
    settings.setRelayPrivateKey(relay_private_key);
    settings.setSeedKey(seed_key);
    settings.sync();

    out << "Configuration successfully created. Don't forget to change your password!" << Qt::endl;
    out << "User name: " << kUserName << Qt::endl;
    out << "Password: " << kPassword << Qt::endl;
    out << "Host public key file: " << host_public_key_file << Qt::endl;
    out << "Relay public key file: " << relay_public_key_file << Qt::endl;

    return 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    LoggingSettings logging_settings;
    logging_settings.min_log_level = LOG_INFO;
#if defined(Q_OS_WINDOWS)
    // The service runs under a low-privilege account that can only write to its own directories, so
    // place the logs next to the application data instead of the system temp directory.
    logging_settings.log_dir = BasePaths::appDataDir() + "/logs";
#endif

    ScopedLogging scoped_logging(logging_settings);

    CoreApplication::setEventDispatcher(new AsioEventDispatcher());
    CoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);

    CoreApplication application(argc, argv);

    QCommandLineOption install_option("install", "Install service.");
    QCommandLineOption remove_option("remove", "Remove service.");
    QCommandLineOption start_option("start", "Start service.");
    QCommandLineOption stop_option("stop", "Stop service.");
    QCommandLineOption keygen_option("keygen", "Generating public and private keys.");
    QCommandLineOption create_config_option("create-config", "Creates a configuration.");

    QCommandLineParser parser;
    parser.addOption(install_option);
    parser.addOption(remove_option);
    parser.addOption(start_option);
    parser.addOption(stop_option);
    parser.addOption(keygen_option);
    parser.addOption(create_config_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    LOG(INFO) << "Version:" << ASPIA_VERSION_STRING << "(arch:" << QSysInfo::buildCpuArchitecture() << ")";
    LOG(INFO) << "Command line:" << CoreApplication::arguments();

    QTextStream out(stdout, QIODevice::WriteOnly);

    if (parser.isSet(keygen_option))
        return generateAndPrintKeys(out);
    else if (parser.isSet(create_config_option))
        return createConfig(out);
    else if (parser.isSet(install_option))
        return installService(out);
    else if (parser.isSet(remove_option))
        return removeService(out);
    else if (parser.isSet(start_option))
        return startService(out);
    else if (parser.isSet(stop_option))
        return stopService(out);

    application.addWorker(std::make_unique<RelayWorker>());
    application.addWorker(std::make_unique<StunWorker>());

    return Service().exec(application);
}
