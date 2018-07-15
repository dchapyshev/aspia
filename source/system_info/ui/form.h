//
// PROJECT:         Aspia
// FILE:            system_info/ui/form.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__UI__FORM_H
#define _ASPIA_SYSTEM_INFO__UI__FORM_H

#include <QWidget>

namespace aspia {

class SystemInfoRequest;

class Form : public QWidget
{
    Q_OBJECT

public:
    virtual ~Form() = default;

    QString uuid() const { return uuid_; }

public slots:
    virtual void readReply(const QString& uuid, const QByteArray& data) = 0;

signals:
    void sendRequest(SystemInfoRequest* request);

protected:
    Form(QWidget* parent, const QString& uuid)
        : QWidget(parent),
          uuid_(uuid)
    {
        // Nothing
    }

private:
    const QString uuid_;
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__UI__FORM_H
