//
// PROJECT:         Aspia
// FILE:            host/win/host_process.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__WIN__HOST_PROCESS_H
#define _ASPIA_HOST__WIN__HOST_PROCESS_H

#include <QObject>
#include <QScopedPointer>

namespace aspia {

class HostProcessImpl;

class HostProcess : public QObject
{
    Q_OBJECT

public:
    enum ProcessState
    {
        NotRunning,
        Starting,
        Running
    };
    Q_ENUM(ProcessState)

    enum Account
    {
        System,
        User
    };
    Q_ENUM(Account)

    enum ErrorCode
    {
        NoError,
        NoLoggedOnUser,
        OtherError
    };
    Q_ENUM(ErrorCode)

    HostProcess(QObject* parent = nullptr);
    virtual ~HostProcess();

    void start(quint32 session_id,
               Account account,
               const QString& program,
               const QStringList& arguments);

    quint32 sessionId() const;
    void setSessionId(quint32 session_id);

    Account account() const;
    void setAccount(Account account);

    QString program() const;
    void setProgram(const QString& program);

    QStringList arguments() const;
    void setArguments(const QStringList& arguments);

    ProcessState state() const;

public slots:
    void start();
    void kill();

signals:
    void started();
    void finished();
    void errorOccurred(HostProcess::ErrorCode error_code);

private:
    friend class HostProcessImpl;

    QScopedPointer<HostProcessImpl> impl_;

    Q_DISABLE_COPY(HostProcess)
};

} // namespace aspia

#endif // _ASPIA_HOST__WIN__HOST_PROCESS_H
