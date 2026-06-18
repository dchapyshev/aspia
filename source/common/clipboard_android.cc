//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/clipboard_android.h"

#include <QClipboard>
#include <QGuiApplication>

//--------------------------------------------------------------------------------------------------
ClipboardAndroid::ClipboardAndroid(QObject* parent)
    : Clipboard(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ClipboardAndroid::init()
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
            this, &ClipboardAndroid::onDataChanged);
}

//--------------------------------------------------------------------------------------------------
void ClipboardAndroid::setData(const QString& mime_type, const QByteArray& data)
{
    if (mime_type != kMimeTypeTextUtf8)
        return;

    QGuiApplication::clipboard()->setText(QString::fromUtf8(data));
}

//--------------------------------------------------------------------------------------------------
void ClipboardAndroid::onDataChanged()
{
    onData(kMimeTypeTextUtf8, QGuiApplication::clipboard()->text().toUtf8());
}
