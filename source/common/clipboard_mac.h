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

#include <mutex>

#include "common/clipboard.h"

class FileDataProvider;
class FilePromiseWriter;

class ClipboardMac final : public Clipboard
{
    Q_OBJECT

public:
    explicit ClipboardMac(QObject* parent = nullptr);
    ~ClipboardMac() final;

    // Clipboard implementation.
    void addFileData(int file_index, const QByteArray& data, bool is_last) final;

protected:
    // Clipboard implementation.
    void init() final;
    void setData(const proto::clipboard::Event& event) final;

private slots:
    void onPollTimer();

private:
    void startTimer();
    void onClipboardData();
    void onClipboardFiles();
    void setDataContent(const proto::clipboard::Event& event);
    void setDataFiles(const QByteArray& data);
    void onFileDataRequested(int file_index, FilePromiseWriter* writer);
    void onFileDataFinished(int file_index, FilePromiseWriter* writer);

    QTimer* timer_ = nullptr;
    int current_change_count_ = 0;
    bool is_setting_data_ = false;

    std::unique_ptr<FileDataProvider> file_data_provider_;
    std::mutex writers_mutex_;
    QMap<int, FilePromiseWriter*> active_writers_;
    bool closing_ = false; // Guarded by |writers_mutex_|.

    Q_DISABLE_COPY(ClipboardMac)
};

#endif // COMMON_CLIPBOARD_MAC_H
