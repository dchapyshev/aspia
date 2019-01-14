//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "common/clipboard.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#include "build/build_config.h"

namespace common {

namespace {

const char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";

} // namespace

Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
            this, &Clipboard::dataChanged);
}

Clipboard::~Clipboard()
{
    disconnect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
               this, &Clipboard::dataChanged);
}

void Clipboard::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (event.mime_type() != kMimeTypeTextUtf8)
        return;

    last_mime_type_ = event.mime_type();
    last_data_ = event.data();

    QString text = QString::fromStdString(event.data());

#if defined(OS_WIN)
    text.replace(QLatin1String("\n"), QLatin1String("\r\n"));
#endif

    QGuiApplication::clipboard()->setText(text);
}

void Clipboard::dataChanged()
{
    const QMimeData* mime_data = QGuiApplication::clipboard()->mimeData();
    if (!mime_data->hasText())
        return;

    proto::desktop::ClipboardEvent event;
    event.set_mime_type(kMimeTypeTextUtf8);
    event.set_data(QString(mime_data->text()).replace(
        QLatin1String("\r\n"), QLatin1String("\n")).toStdString());

    if (event.mime_type() == last_mime_type_ && event.data() == last_data_)
        return;

    emit clipboardEvent(event);
}

} // namespace common
