//
// PROJECT:         Aspia
// FILE:            system_info/ui/parser.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__UI__PARSER_H
#define _ASPIA_SYSTEM_INFO__UI__PARSER_H

#include <QWidget>

namespace aspia {

class SystemInfoRequest;

class Parser : public QWidget
{
public:
    explicit Parser(QWidget* parent)
        : QWidget(parent)
    {
        // Nothing
    }

    virtual ~Parser() = default;

signals:
    void sendRequest(SystemInfoRequest* request);

public slots:
    virtual void readReply(const QString& uuid, const QByteArray& data) = 0;
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__UI__PARSER_H
