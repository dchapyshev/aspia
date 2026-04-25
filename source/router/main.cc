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

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/service_controller.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/random.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/peer/user.h"
#include "build/version.h"
#include "router/database.h"
#include "router/service.h"
#include "router/settings.h"

namespace {

//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    std::unique_ptr<base::ServiceController> controller = base::ServiceController::open(router::Service::kName);
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
    std::unique_ptr<base::ServiceController> controller = base::ServiceController::open(router::Service::kName);
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
    std::unique_ptr<base::ServiceController> controller = base::ServiceController::install(
        router::Service::kName, router::Service::kDisplayName, QCoreApplication::applicationFilePath());
    if (!controller)
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller->setDescription(router::Service::kDescription);
    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (base::ServiceController::isRunning(router::Service::kName))
        stopService(out);

    if (!base::ServiceController::remove(router::Service::kName))
    {
        out << "Failed to remove the service." << Qt::endl;
        return 1;
    }

    out << "The service was successfully deleted." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
bool generateKeys(QByteArray* private_key, QByteArray* public_key)
{
    base::KeyPair key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
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
    QByteArray private_key;
    QByteArray public_key;

    if (!generateKeys(&private_key, &public_key))
        return 1;

    out << "Private key: " << private_key.toHex() << Qt::endl;
    out << "Public key: " << public_key.toHex() << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
QString publicKeyDirectory()
{
    return base::BasePaths::appConfigDir();
}

//--------------------------------------------------------------------------------------------------
int createConfig(QTextStream& out)
{
    out << "Creation of initial configuration started." << Qt::endl;

    router::Settings settings;
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

    QString public_key_file = public_key_dir;
    public_key_file.append("/router.pub");

    out << "Public key file: " << public_key_file << Qt::endl;

    if (QFileInfo::exists(public_key_file))
    {
        out << "Public key file already exists. Continuation is impossible." << Qt::endl;
        return 1;
    }
    else
    {
        out << "Public key does not exist yet." << Qt::endl;
    }

    router::Database db = router::Database::create();
    if (!db.isValid())
    {
        db = router::Database::open();
        if (db.isValid())
            out << "Database already exists. Continuation is impossible." << Qt::endl;
        else
            out << "Failed to create new database." << Qt::endl;
        return 1;
    }

    out << "Creating a user..." << Qt::endl;

    const char kUserName[] = "admin";
    const char kPassword[] = "admin";

    base::User user = base::User::create(kUserName, kPassword);
    if (!user.isValid())
    {
        out << "Failed to create user." << Qt::endl;
        return 1;
    }

    out << "User has been created. Adding a user to the database..." << Qt::endl;

    user.sessions = proto::router::SESSION_TYPE_ADMIN | proto::router::SESSION_TYPE_CLIENT;
    user.flags = base::User::ENABLED;

    if (!db.addUser(user))
    {
        out << "Failed to add user to database." << Qt::endl;
        return 1;
    }

    out << "User was successfully added to the database." << Qt::endl;
    out << "Generating encryption keys..." << Qt::endl;

    QByteArray private_key;
    QByteArray public_key;
    if (!generateKeys(&private_key, &public_key))
        return 1;

    out << "Private and public keys have been successfully generated." << Qt::endl;
    out << "Writing a public key to a file..." << Qt::endl;

    if (!base::writeFile(public_key_file, public_key.toHex()))
    {
        out << "Failed to write public key to file: " << public_key_file << Qt::endl;
        return 1;
    }

    out << "Generate seed key..." << Qt::endl;
    QByteArray seed_key = base::Random::byteArray(64);
    if (seed_key.isEmpty())
        out << "Unable to generate seed key" << Qt::endl;

    out << "Seed key successfully generated" << Qt::endl;

    // Save the configuration file.
    settings.reset();
    settings.setPrivateKey(private_key);
    settings.setSeedKey(seed_key);
    settings.sync();

    out << "Configuration successfully created. Don't forget to change your password!" << Qt::endl;
    out << "User name: " << kUserName << Qt::endl;
    out << "Password: " << kPassword << Qt::endl;
    out << "Public key file: " << public_key_file << Qt::endl;

    return 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    base::Application::setEventDispatcher(new base::AsioEventDispatcher());
    base::Application::setApplicationVersion(ASPIA_VERSION_STRING);

    base::Application application(argc, argv);

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
    LOG(INFO) << "Command line:" << base::Application::arguments();

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

    return router::Service().exec(application);
}
