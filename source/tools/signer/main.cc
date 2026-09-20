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

#include <QFile>

#include <iostream>

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/signature.h"
#include "base/files/file_util.h"

namespace {

const qsizetype kKeySize = 32;

//--------------------------------------------------------------------------------------------------
void printUsage()
{
    std::cout << "Signs the files of a release with the key the applications carry." << std::endl
              << std::endl
              << "  aspia_signer genkey <key-file>    creates a key and prints its public half"
              << std::endl
              << "  aspia_signer sign <key-file> <file>...    writes <file>.sig next to each file"
              << std::endl
              << "  aspia_signer verify <key> <file>...    checks each <file>.sig, where <key>"
              << " is a key file or a public key in hex"
              << std::endl;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray readKey(const QString& file_path)
{
    QByteArray buffer;
    if (!readFile(file_path, &buffer))
    {
        std::cerr << "Unable to read key file" << std::endl;
        return SecureByteArray();
    }

    SecureByteArray key(QByteArray::fromHex(buffer.trimmed()));
    if (key.size() != kKeySize)
    {
        std::cerr << "Key file does not hold a key" << std::endl;
        return SecureByteArray();
    }

    return key;
}

//--------------------------------------------------------------------------------------------------
// What is checked against is the public key. The private key is taken when it is at hand, so that
// the same file works for signing and for checking, and a published release is checked with nothing
// but the key the applications carry.
QByteArray readPublicKey(const QString& key)
{
    if (QFile::exists(key))
    {
        SecureByteArray private_key = readKey(key);
        if (private_key.isEmpty())
            return QByteArray();

        return Signature::publicKey(private_key);
    }

    QByteArray public_key = QByteArray::fromHex(key.toLatin1());
    if (public_key.size() != kKeySize)
    {
        std::cerr << "Argument is neither a key file nor a public key" << std::endl;
        return QByteArray();
    }

    return public_key;
}

//--------------------------------------------------------------------------------------------------
bool genKey(const QString& file_path)
{
    if (QFile::exists(file_path))
    {
        std::cerr << "File already exists. A key is never replaced, it is kept forever."
                  << std::endl;
        return false;
    }

    SecureByteArray private_key(Random::byteArray(kKeySize));

    QByteArray public_key = Signature::publicKey(private_key);
    if (public_key.isEmpty())
        return false;

    if (!writeFile(file_path, private_key.toByteArray().toHex()))
    {
        std::cerr << "Unable to write key file" << std::endl;
        return false;
    }

    QFile::setPermissions(file_path, QFile::ReadOwner | QFile::WriteOwner);

    std::cout << "Public key: " << public_key.toHex().toStdString() << std::endl;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool sign(const SecureByteArray& private_key, const QString& file_path)
{
    QByteArray data;
    if (!readFile(file_path, &data))
    {
        std::cerr << "Unable to read file" << std::endl;
        return false;
    }

    QByteArray signature = Signature::create(private_key, data);
    if (signature.isEmpty())
        return false;

    QString signature_path = file_path + ".sig";

    if (!writeFile(signature_path, Signature::tagged(signature)))
    {
        std::cerr << "Unable to write signature file" << std::endl;
        return false;
    }

    std::cout << signature_path.toStdString() << std::endl;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool verify(const QByteArray& public_key, const QString& file_path)
{
    QByteArray data;
    if (!readFile(file_path, &data))
    {
        std::cerr << "Unable to read file" << std::endl;
        return false;
    }

    QByteArray signature;
    if (!readFile(file_path + ".sig", &signature))
    {
        std::cerr << "Unable to read signature file" << std::endl;
        return false;
    }

    if (!Signature::verify(public_key, data, Signature::untagged(signature)))
    {
        std::cout << file_path.toStdString() << " NOT SIGNED BY THIS KEY" << std::endl;
        return false;
    }

    std::cout << file_path.toStdString() << " ok" << std::endl;
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    LoggingSettings settings;
    settings.destination = LOG_TO_STDOUT;
    initLogging(settings);

    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    if (arguments.size() < 2)
    {
        printUsage();
        return 1;
    }

    QString command = arguments.takeFirst();
    QString key_path = arguments.takeFirst();

    if (command == "genkey")
    {
        if (!arguments.isEmpty())
        {
            printUsage();
            return 1;
        }

        return genKey(key_path) ? 0 : 1;
    }

    if (arguments.isEmpty())
    {
        printUsage();
        return 1;
    }

    bool result = true;

    if (command == "sign")
    {
        SecureByteArray private_key = readKey(key_path);
        if (private_key.isEmpty())
            return 1;

        for (const QString& file_path : arguments)
            result &= sign(private_key, file_path);
    }
    else if (command == "verify")
    {
        QByteArray public_key = readPublicKey(key_path);
        if (public_key.isEmpty())
            return 1;

        for (const QString& file_path : arguments)
            result &= verify(public_key, file_path);
    }
    else
    {
        printUsage();
        return 1;
    }

    return result ? 0 : 1;
}
