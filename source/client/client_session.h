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
    // Reads the incoming message for the session.
    virtual void messageReceived(const QByteArray& buffer) = 0;

    // Message with ID |message_id| sent.
    virtual void messageWritten(int message_id) = 0;

    // Starts session.
    virtual void startSession() = 0;

    // Closes the session. When a slot is called, signal |sessionClosed| is not generated.
    virtual void closeSession() = 0;

signals:
    // Indicates an outgoing message.
    void writeMessage(int message_id, const QByteArray& buffer);

    // Indicates that it is ready to receive the next incoming message.
    void readMessage();

    // Indicates an error in the session.
    void errorOccurred(const QString& message);

    // Indicates the end of the session by the user (for example, when the session window
    // is closed).
    void closedByUser();
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
