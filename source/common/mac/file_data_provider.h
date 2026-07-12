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

#ifndef COMMON_MAC_FILE_DATA_PROVIDER_H
#define COMMON_MAC_FILE_DATA_PROVIDER_H

#include <functional>

#include <QString>

#include "proto/desktop_clipboard.h"

#ifdef __OBJC__
@class NSMutableArray;
#else
typedef void NSMutableArray;
#endif

class FilePromiseWriter;

// Places empty temporary files on NSPasteboard and registers NSFilePresenter
// for each one. When the user actually pastes (Finder reads the file via
// NSFileCoordinator), relinquishPresentedItemToReader: fires and the file
// data is downloaded on-demand from the remote side. No data is transferred
// until the user explicitly pastes.
class FileDataProvider final
{
public:
    using FileDataRequestCallback = std::function<void(int file_index, FilePromiseWriter* writer)>;
    using FileDataFinishedCallback = std::function<void(int file_index, FilePromiseWriter* writer)>;

    ~FileDataProvider();

    static FileDataProvider* create(const proto::clipboard::Event::FileList& files,
                                    FileDataRequestCallback request_callback,
                                    FileDataFinishedCallback finished_callback);

    // Creates empty temp files, registers NSFilePresenters, writes file URLs
    // to NSPasteboard. This is instant — no network transfer occurs.
    void writeToPasteboard();

    // Called from NSFilePresenter (on background queue) when file data is needed.
    void onFileRequested(int file_index, FilePromiseWriter* writer);

    // Called from NSFilePresenter (on background queue) right before |writer| is destroyed, so the
    // subscriber can drop its reference to it.
    void onFileFinished(int file_index, FilePromiseWriter* writer);

    const proto::clipboard::Event::FileList& files() const { return files_; }
    const QString& tempDir() const { return temp_dir_; }

private:
    FileDataProvider(const proto::clipboard::Event::FileList& files,
                     FileDataRequestCallback request_callback,
                     FileDataFinishedCallback finished_callback);

    FileDataRequestCallback request_callback_;
    FileDataFinishedCallback finished_callback_;
    proto::clipboard::Event::FileList files_;
    QString temp_dir_;

#ifdef __OBJC__
    NSMutableArray* file_presenters_ = nullptr;
#else
    void* file_presenters_ = nullptr;
#endif

    Q_DISABLE_COPY_MOVE(FileDataProvider)
};

#endif // COMMON_MAC_FILE_DATA_PROVIDER_H
