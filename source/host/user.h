//
// PROJECT:         Aspia
// FILE:            host/user.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__USER_H
#define _ASPIA_HOST__USER_H

#include <QByteArray>
#include <QString>

namespace aspia {

class User
{
public:
    enum Flags { FLAG_ENABLED = 1 };

    static bool isValidName(const QString& value);
    static bool isValidPassword(const QString& value);

    bool setName(const QString& value);
    const QString& name() const { return name_; }

    bool setPassword(const QString& value);
    bool setPasswordHash(const QByteArray& value);
    const QByteArray& passwordHash() const { return password_hash_; }

    void setFlags(quint32 value);
    quint32 flags() const { return flags_; }

    void setSessions(quint32 value);
    quint32 sessions() const { return sessions_; }

private:
    QString name_;
    QByteArray password_hash_;
    quint32 flags_ = 0;
    quint32 sessions_ = 0;
};

} // namespace aspia

#endif // _ASPIA_HOST__USER_H
