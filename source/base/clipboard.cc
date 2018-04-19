//
// PROJECT:         Aspia
// FILE:            base/clipboard.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/clipboard.h"

#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QMimeData>

namespace aspia {

namespace {

const char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";

} // namespace

Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, [this]()
    {
        const QMimeData* mime_data = QGuiApplication::clipboard()->mimeData();
        if (!mime_data->hasText())
            return;

        proto::desktop::ClipboardEvent event;
        event.set_mime_type(kMimeTypeTextUtf8);
        event.set_data(QString(mime_data->text()).replace(
            QStringLiteral("\r\n"), QStringLiteral("\n")).toStdString());

        if (event.mime_type() == last_mime_type_ && event.data() == last_data_)
            return;

        emit clipboardEvent(event);
    });
}

Clipboard::~Clipboard() = default;

void Clipboard::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (event.mime_type() != kMimeTypeTextUtf8)
        return;

    last_mime_type_ = event.mime_type();
    last_data_ = event.data();

    QString text = QString::fromStdString(event.data());

#if defined(Q_OS_WIN)
    text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
#endif

    QGuiApplication::clipboard()->setText(text);
}

} // namespace aspia
