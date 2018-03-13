//
// PROJECT:         Aspia
// FILE:            client/client_session.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_H
#define _ASPIA_CLIENT__CLIENT_SESSION_H

#include <QObject>

namespace aspia {

class ClientSession : public QObject
{
    Q_OBJECT

public:
    ClientSession(QObject* parent) : QObject(parent) {}
    virtual ~ClientSession() = default;

public slots:
    virtual void readMessage(const QByteArray& buffer) = 0;
    virtual void close() = 0;

signals:
    void sessionMessage(const QByteArray& buffer);
    void sessionError(const QString& message);
    void sessionClose();
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
