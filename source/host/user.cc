//
// PROJECT:         Aspia
// FILE:            host/user.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/user.h"

#include <QCryptographicHash>

#include "crypto/secure_memory.h"

namespace aspia {

namespace {

bool isValidUserNameChar(const QChar& username_char)
{
    if (username_char.isLetter())
        return true;

    if (username_char.isDigit())
        return true;

    if (username_char == '.' ||
        username_char == '_' ||
        username_char == '-')
    {
        return true;
    }

    return false;
}

bool isValidPasswordHash(const QByteArray& password_hash)
{
    if (password_hash.size() != User::kPasswordHashLength)
        return false;

    return true;
}

QByteArray createPasswordHash(const QString& password)
{
    static const int kIterCount = 100000;

    QByteArray data = password.toUtf8();

    for (int i = 0; i < kIterCount; ++i)
    {
        data = QCryptographicHash::hash(data, QCryptographicHash::Sha512);
    }

    return data;
}

} // namespace

User::~User()
{
    secureMemZero(&name_);
    secureMemZero(&password_hash_);
}

// static
bool User::isValidName(const QString& value)
{
    int length = value.length();

    if (length <= 0 || length > kMaxUserNameLength)
        return false;

    for (int i = 0; i < length; ++i)
    {
        if (!isValidUserNameChar(value[i]))
            return false;
    }

    return true;
}

// static
bool User::isValidPassword(const QString& value)
{
    int length = value.length();

    if (length < kMinPasswordLength || length > kMaxPasswordLength)
        return false;

    return true;
}

bool User::setName(const QString& value)
{
    if (!isValidName(value))
        return false;

    name_ = value;
    return true;
}

bool User::setPassword(const QString& value)
{
    if (!isValidPassword(value))
        return false;

    password_hash_ = createPasswordHash(value);
    return true;
}

bool User::setPasswordHash(const QByteArray& value)
{
    if (!isValidPasswordHash(value))
        return false;

    password_hash_ = value;
    return true;
}

void User::setFlags(quint32 value)
{
    flags_ = value;
}

void User::setSessions(quint32 value)
{
    sessions_ = value;
}

} // namespace aspia
