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
#include <QMimeData>

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
void ClipboardAndroid::setData(const proto::clipboard::Event& event)
{
    QClipboard* clipboard = QGuiApplication::clipboard();

    const proto::clipboard::Event::Format* text_format = findFormat(event, kMimeTypeTextUtf8);
    const proto::clipboard::Event::Format* html_format = findFormat(event, kMimeTypeTextHtml);
    if (!text_format && !html_format)
    {
        // An empty event is the clear command: drop the content along with the echo state.
        clipboard->clear();
        injected_text_.clear();
        injected_html_.clear();
        has_injected_data_ = false;
        return;
    }

    injected_text_ = text_format ? QString::fromStdString(text_format->data()) : QString();
    injected_html_ = html_format ? QString::fromStdString(html_format->data()) : QString();
    has_injected_data_ = true;

    // Both formats describe one clipboard change; Qt publishes them as a single ClipData with
    // the plain text and its HTML markup.
    QMimeData* mime_data = new QMimeData();
    if (text_format)
        mime_data->setText(injected_text_);
    if (html_format)
        mime_data->setHtml(injected_html_);

    // The clipboard takes ownership of |mime_data|.
    clipboard->setMimeData(mime_data);

    // Calibrate the echo state with what was actually stored (the platform may normalize the
    // content). A denied read-back (Android restricts clipboard reads to the focused app) must
    // not wipe the calibration: empty values would make the suppression in onDataChanged() match
    // every text-only content and silently withhold the user's own copies.
    const QMimeData* stored = clipboard->mimeData();
    if (stored && (stored->hasText() || stored->hasHtml()))
    {
        injected_text_ = stored->hasText() ? stored->text() : QString();
        injected_html_ = stored->hasHtml() ? stored->html() : QString();
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardAndroid::onDataChanged()
{
    const QMimeData* mime_data = QGuiApplication::clipboard()->mimeData();
    if (!mime_data)
        return;

    const QString text = mime_data->hasText() ? mime_data->text() : QString();
    const QString html = mime_data->hasHtml() ? mime_data->html() : QString();

    if (has_injected_data_ && html == injected_html_ &&
        (text == injected_text_ || injected_text_.isEmpty()))
    {
        return;
    }

    proto::clipboard::Event event;
    addFormat(&event, kMimeTypeTextUtf8, text.toUtf8());
    addFormat(&event, kMimeTypeTextHtml, html.toUtf8());

    onData(std::move(event));
}
