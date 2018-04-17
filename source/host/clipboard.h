//
// PROJECT:         Aspia
// FILE:            host/clipboard.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__CLIPBOARD_H
#define _ASPIA_HOST__CLIPBOARD_H

#include <QObject>

#include "protocol/desktop_session.pb.h"

namespace aspia {

class Clipboard : public QObject
{
    Q_OBJECT

public:
    Clipboard(QObject* parent = nullptr);
    ~Clipboard();

public slots:
    // Receiving the incoming clipboard.
    void injectClipboardEvent(const proto::desktop::ClipboardEvent& event);

signals:
    void clipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    std::string last_mime_type_;
    std::string last_data_;

    Q_DISABLE_COPY(Clipboard)
};

} // namespace aspia

#endif // _ASPIA_HOST__CLIPBOARD_H
