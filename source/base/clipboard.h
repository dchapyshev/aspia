//
// PROJECT:         Aspia
// FILE:            base/clipboard.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__CLIPBOARD_H
#define _ASPIA_BASE__CLIPBOARD_H

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

private slots:
    void dataChanged();

private:
    std::string last_mime_type_;
    std::string last_data_;

    Q_DISABLE_COPY(Clipboard)
};

} // namespace aspia

#endif // _ASPIA_BASE__CLIPBOARD_H
