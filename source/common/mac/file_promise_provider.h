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

#ifndef COMMON_MAC_FILE_PROMISE_PROVIDER_H
#define COMMON_MAC_FILE_PROMISE_PROVIDER_H

#include <functional>

#include <QMap>

#include "proto/desktop_clipboard.h"

#ifdef __OBJC__
@class FilePromiseDelegate;
@class NSArray;
@class NSOperationQueue;
#else
typedef void FilePromiseDelegate;
typedef void NSArray;
typedef void NSOperationQueue;
#endif

namespace common {

class FilePromiseWriter;

// macOS analogue of FileObject (Windows IDataObject).
//
// Creates NSFilePromiseProvider objects for each file in the list and a delegate
// that handles Finder's paste requests. When Finder calls writePromiseTo: for a file,
// the delegate creates a FilePromiseWriter and invokes the callback so that
// ClipboardMac can emit sig_fileDataRequest and route incoming data to the writer.
class FilePromiseProvider final
{
public:
    using FileDataRequestCallback = std::function<void(int file_index, FilePromiseWriter* writer)>;

    ~FilePromiseProvider();

    static FilePromiseProvider* create(const proto::clipboard::Event::FileList& files,
                                       FileDataRequestCallback callback);

    // Returns the array of NSFilePromiseProvider objects to place on NSPasteboard.
    NSArray* providers() const;

    // Called by the delegate from NSOperationQueue thread when Finder requests a file.
    // Routes through to the callback on the Qt thread.
    void onFileRequested(int file_index, FilePromiseWriter* writer);

private:
    FilePromiseProvider(const proto::clipboard::Event::FileList& files,
                        FileDataRequestCallback callback);

    FileDataRequestCallback callback_;
    proto::clipboard::Event::FileList files_;

#ifdef __OBJC__
    FilePromiseDelegate* delegate_ = nil;
    NSArray* providers_ = nil;
    NSOperationQueue* operation_queue_ = nil;
#else
    void* delegate_ = nullptr;
    void* providers_ = nullptr;
    void* operation_queue_ = nullptr;
#endif

    Q_DISABLE_COPY_MOVE(FilePromiseProvider)
};

} // namespace common

#endif // COMMON_MAC_FILE_PROMISE_PROVIDER_H
