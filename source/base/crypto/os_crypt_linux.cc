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

#include "base/crypto/os_crypt.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"

// Linux (Android included) has no keystore that every system exposes to native code without a new
// dependency. The wrap key is a random 32-byte key in a file only the user can read
// (~/.config/aspia/os_crypt.key, 0600 in a 0700 directory), created on first use.
//
// Older versions derived the key from the machine id (/etc/machine-id for an unprivileged
// process), which every local user can read. Such seals still open (decryptBytes() reports
// |needs_reencrypt|), and the callers store them again under the random key.

namespace {

const char kContext[] = "base::OSCrypt::key";
const char kAad[] = "base::OSCrypt";
const qsizetype kKeySize = 32;

QString g_key_file_for_testing;

//--------------------------------------------------------------------------------------------------
QString keyFilePath()
{
    if (!g_key_file_for_testing.isEmpty())
        return g_key_file_for_testing;

    const QString config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (config_dir.isEmpty())
        return QString();

    return config_dir + QStringLiteral("/aspia/os_crypt.key");
}

//--------------------------------------------------------------------------------------------------
SecureByteArray readKeyFile(const QString& path)
{
    const std::string native = QFile::encodeName(path).toStdString();

    int fd = ::open(native.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return SecureByteArray();

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        st.st_size != kKeySize)
    {
        LOG(ERROR) << "Key file is not a private regular file of this user:" << path;
        ::close(fd);
        return SecureByteArray();
    }

    // A key readable by others is still the key of this user; take it back to 0600.
    if ((st.st_mode & 0077) != 0 && fchmod(fd, 0600) != 0)
        PLOG(WARNING) << "Unable to restrict permissions of" << path;

    QByteArray buffer(kKeySize, '\0');
    qsizetype done = 0;
    while (done < kKeySize)
    {
        const ssize_t count = ::read(fd, buffer.data() + done, static_cast<size_t>(kKeySize - done));
        if (count <= 0)
        {
            if (count < 0 && errno == EINTR)
                continue;
            ::close(fd);
            memZero(&buffer);
            return SecureByteArray();
        }
        done += count;
    }

    ::close(fd);
    return SecureByteArray(std::move(buffer));
}

//--------------------------------------------------------------------------------------------------
SecureByteArray createKeyFile(const QString& path)
{
    const QString dir_path = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir_path))
    {
        LOG(ERROR) << "Unable to create" << dir_path;
        return SecureByteArray();
    }

    const std::string native_dir = QFile::encodeName(dir_path).toStdString();
    if (::chmod(native_dir.c_str(), 0700) != 0)
        PLOG(WARNING) << "Unable to restrict permissions of" << dir_path;

    QByteArray random = Random::byteArray(static_cast<size_t>(kKeySize));
    if (random.size() != kKeySize)
        return SecureByteArray();
    SecureByteArray key(std::move(random));

    // O_EXCL: another process that created the file first wins, its key is read back.
    const std::string native = QFile::encodeName(path).toStdString();
    int fd = ::open(native.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0)
    {
        if (errno == EEXIST)
            return readKeyFile(path);
        PLOG(ERROR) << "Unable to create" << path;
        return SecureByteArray();
    }

    qsizetype done = 0;
    while (done < kKeySize)
    {
        const ssize_t count = ::write(fd, key.constData() + done, static_cast<size_t>(kKeySize - done));
        if (count <= 0)
        {
            if (count < 0 && errno == EINTR)
                continue;
            PLOG(ERROR) << "Unable to write" << path;
            ::close(fd);
            ::unlink(native.c_str());
            return SecureByteArray();
        }
        done += count;
    }

    fsync(fd);
    ::close(fd);
    return key;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray fileKey(bool create)
{
    const QString path = keyFilePath();
    if (path.isEmpty())
    {
        LOG(ERROR) << "No configuration directory for the key file";
        return SecureByteArray();
    }

    SecureByteArray key = readKeyFile(path);
    if (key.isEmpty() && create && !QFileInfo::exists(path))
        key = createKeyFile(path);
    return key;
}

//--------------------------------------------------------------------------------------------------
// The key of older versions: derived from public data (the machine id). Only for opening.
SecureByteArray legacyKey()
{
    const QByteArray hardware_id = SysInfo::hardwareId();
    if (hardware_id.isEmpty())
        return SecureByteArray();

    return SecureByteArray(GenericHash::hash(GenericHash::SHA256, QByteArray(kContext) + hardware_id));
}

//--------------------------------------------------------------------------------------------------
bool seal(const SecureByteArray& key, const QByteArray& plaintext, QByteArray* ciphertext)
{
    if (key.isEmpty())
        return false;

    const DataCryptor cryptor(key);
    std::optional<QByteArray> sealed = cryptor.encrypt(plaintext, kAad);
    if (!sealed.has_value())
        return false;

    *ciphertext = std::move(*sealed);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool openSeal(const SecureByteArray& key, const QByteArray& ciphertext, QByteArray* plaintext)
{
    if (key.isEmpty())
        return false;

    const DataCryptor cryptor(key);
    std::optional<QByteArray> opened = cryptor.decrypt(ciphertext, kAad);
    if (!opened.has_value())
        return false;

    *plaintext = std::move(*opened);
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::encryptBytes(const QByteArray& plaintext, QByteArray* ciphertext)
{
    if (plaintext.isEmpty())
    {
        *ciphertext = QByteArray();
        return true;
    }

    if (!seal(fileKey(/* create */ true), plaintext, ciphertext))
    {
        LOG(ERROR) << "Failed to encrypt";
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::decryptBytes(const QByteArray& ciphertext, QByteArray* plaintext, bool* needs_reencrypt)
{
    if (needs_reencrypt)
        *needs_reencrypt = false;

    if (ciphertext.isEmpty())
    {
        *plaintext = QByteArray();
        return true;
    }

    if (openSeal(fileKey(/* create */ false), ciphertext, plaintext))
        return true;

    if (openSeal(legacyKey(), ciphertext, plaintext))
    {
        LOG(INFO) << "Opened a seal of the outdated machine-id key; it is to be stored again";
        if (needs_reencrypt)
            *needs_reencrypt = true;
        return true;
    }

    LOG(ERROR) << "Failed to decrypt";
    return false;
}

//--------------------------------------------------------------------------------------------------
// static
void OSCrypt::setKeyFilePathForTesting(const QString& path)
{
    g_key_file_for_testing = path;
}

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::encryptBytesLegacyForTesting(const QByteArray& plaintext, QByteArray* ciphertext)
{
    return seal(legacyKey(), plaintext, ciphertext);
}
