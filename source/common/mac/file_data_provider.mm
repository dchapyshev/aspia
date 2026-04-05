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

#include "common/mac/file_data_provider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "base/logging.h"
#include "common/mac/file_promise_writer.h"

#import <Cocoa/Cocoa.h>

// ---------------------------------------------------------------------------
// AsFilePresenter — NSFilePresenter for on-demand file download.
//
// When Finder pastes a file, it uses NSFileCoordinator to read it. This
// triggers relinquishPresentedItemToReader: on our presenter's operation
// queue (background thread). We then download the file data from the remote
// side and write it to the temp file before letting the reader proceed.
// The main thread is never blocked.
// ---------------------------------------------------------------------------

@interface AsFilePresenter : NSObject <NSFilePresenter>

- (instancetype)initWithFileURL:(NSURL*)url
                      fileIndex:(int)fileIndex
                       provider:(common::FileDataProvider*)provider;
- (void)stopPresenting;

@end

@implementation AsFilePresenter
{
    NSURL* _url;
    int _file_index;
    common::FileDataProvider* _provider;
    NSOperationQueue* _queue;
}

- (instancetype)initWithFileURL:(NSURL*)url
                      fileIndex:(int)fileIndex
                       provider:(common::FileDataProvider*)provider
{
    self = [super init];
    if (self)
    {
        _url = url;
        _file_index = fileIndex;
        _provider = provider;

        _queue = [[NSOperationQueue alloc] init];
        _queue.maxConcurrentOperationCount = 1;

        [NSFileCoordinator addFilePresenter:self];
    }
    return self;
}

- (void)stopPresenting
{
    [NSFileCoordinator removeFilePresenter:self];
}

- (NSURL*)presentedItemURL
{
    return _url;
}

- (NSOperationQueue*)presentedItemOperationQueue
{
    return _queue;
}

- (void)relinquishPresentedItemToReader:(void (^)(void (^ _Nullable)(void)))reader
{
    const auto& file = _provider->files().file(_file_index);

    LOG(INFO) << "relinquishPresentedItemToReader for file index:" << _file_index
              << "path:" << QString::fromStdString(file.path());

    // Create a writer that will block until all data is received.
    auto* writer = new common::FilePromiseWriter(file.file_size());

    // Signal ClipboardMac to request data from the remote side.
    _provider->onFileRequested(_file_index, writer);

    // Block this background thread until the file is fully downloaded.
    QString dest_path = QString(_provider->tempDir() + '/' +
                                QString::fromStdString(file.path()));
    bool success = writer->writeToDestination(dest_path);
    delete writer;

    if (!success)
        LOG(ERROR) << "Failed to download file index:" << _file_index;

    // Let the reader (Finder) proceed now that the file has real data.
    reader(^{ });

    [self stopPresenting];
}

@end

// ---------------------------------------------------------------------------
// FileDataProvider C++ implementation.
// ---------------------------------------------------------------------------

namespace common {

//--------------------------------------------------------------------------------------------------
FileDataProvider::FileDataProvider(const proto::clipboard::Event::FileList& files,
                                   FileDataRequestCallback callback)
    : callback_(std::move(callback)),
      files_(files)
{
    temp_dir_ = QDir::tempPath() + "/aspia-clipboard-" +
                QString::number(reinterpret_cast<quintptr>(this), 16);
    QDir().mkpath(temp_dir_);

    file_presenters_ = [[NSMutableArray alloc] init];

    LOG(INFO) << "Created FileDataProvider with" << files_.file_size()
              << "entries, temp dir:" << temp_dir_;
}

//--------------------------------------------------------------------------------------------------
FileDataProvider::~FileDataProvider()
{
    LOG(INFO) << "Destroying FileDataProvider, cleaning up";

    // Remove all file presenters.
    for (AsFilePresenter* presenter in file_presenters_)
        [presenter stopPresenting];
    [file_presenters_ removeAllObjects];
    file_presenters_ = nil;

    // Clean temp directory.
    if (!temp_dir_.isEmpty())
        QDir(temp_dir_).removeRecursively();
}

//--------------------------------------------------------------------------------------------------
// static
FileDataProvider* FileDataProvider::create(const proto::clipboard::Event::FileList& files,
                                           FileDataRequestCallback callback)
{
    if (files.file_size() == 0)
    {
        LOG(ERROR) << "Empty file list";
        return nullptr;
    }

    return new FileDataProvider(files, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void FileDataProvider::writeToPasteboard()
{
    NSMutableArray<NSURL*>* top_level_urls = [NSMutableArray array];

    for (int i = 0; i < files_.file_size(); ++i)
    {
        const auto& file = files_.file(i);
        QString path = QString::fromStdString(file.path());
        QString full_path = QString(temp_dir_ + '/' + path);

        if (file.is_dir())
        {
            // Create directory structure.
            QDir().mkpath(full_path);
        }
        else
        {
            // Create parent directories if needed.
            QDir().mkpath(QFileInfo(full_path).absolutePath());

            // Create empty placeholder file.
            QFile f(full_path);
            if (f.open(QIODevice::WriteOnly))
                f.close();

            // Register NSFilePresenter for on-demand download.
            NSURL* url = [NSURL fileURLWithPath:full_path.toNSString() isDirectory:NO];
            AsFilePresenter* presenter = [[AsFilePresenter alloc] initWithFileURL:url
                                                                       fileIndex:i
                                                                        provider:this];
            [file_presenters_ addObject:presenter];
        }

        // Only top-level entries go on the pasteboard (no '/' in path).
        if (!path.contains('/'))
        {
            NSURL* url = [NSURL fileURLWithPath:full_path.toNSString()
                                    isDirectory:file.is_dir()];
            [top_level_urls addObject:url];
        }
    }

    // Write file URLs to pasteboard.
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:top_level_urls];

    LOG(INFO) << "Placed" << [top_level_urls count] << "top-level URLs on pasteboard from"
              << files_.file_size() << "total entries";
}

//--------------------------------------------------------------------------------------------------
void FileDataProvider::onFileRequested(int file_index, FilePromiseWriter* writer)
{
    if (callback_)
        callback_(file_index, writer);
}

} // namespace common
