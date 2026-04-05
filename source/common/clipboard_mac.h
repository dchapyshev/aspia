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

#ifndef COMMON_CLIPBOARD_MAC_H
#define COMMON_CLIPBOARD_MAC_H

#include <QMap>
#include <QTimer>

#include "common/clipboard.h"

namespace common {

class FilePromiseProvider;
class FilePromiseWriter;

class ClipboardMac final : public Clipboard
{
    Q_OBJECT

public:
    explicit ClipboardMac(QObject* parent = nullptr);
    ~ClipboardMac();

    // Clipboard implementation.
    void addFileData(int file_index, const QByteArray& data, bool is_last) final;

protected:
    // Clipboard implementation.
    void init() final;
    void setData(const QString& mime_type, const QByteArray& data) final;

private:
    void startTimer();
    void checkForChanges();
    void onClipboardText();
    void onClipboardFiles();
    void setDataText(const QByteArray& data);
    void setDataFiles(const QByteArray& data);
    void onFileDataRequested(int file_index, FilePromiseWriter* writer);

    QTimer* timer_ = nullptr;
    int current_change_count_ = 0;

    std::unique_ptr<FilePromiseProvider> file_promise_provider_;
    QMap<int, FilePromiseWriter*> active_writers_;

    Q_DISABLE_COPY(ClipboardMac)
};

} // namespace common

#endif // COMMON_CLIPBOARD_MAC_H
