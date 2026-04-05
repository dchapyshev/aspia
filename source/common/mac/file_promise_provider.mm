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

#include "common/mac/file_promise_provider.h"

#include <QDir>

#include "base/logging.h"
#include "common/mac/file_promise_writer.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

// ---------------------------------------------------------------------------
// FilePromiseDelegate — NSFilePromiseProviderDelegate implementation.
//
// Each NSFilePromiseProvider stores its file_index in userInfo. When Finder
// pastes, it calls the delegate methods on the operation queue thread:
//   1) fileNameForType:  -> returns the relative file path
//   2) writePromiseTo:   -> blocks while data is transferred via FilePromiseWriter
// ---------------------------------------------------------------------------

@interface FilePromiseDelegate : NSObject <NSFilePromiseProviderDelegate>

- (instancetype)initWithProvider:(common::FilePromiseProvider*)provider
                           files:(const proto::clipboard::Event::FileList&)files
                  operationQueue:(NSOperationQueue*)queue;

@end

@implementation FilePromiseDelegate
{
    common::FilePromiseProvider* _provider;
    proto::clipboard::Event::FileList _files;
    NSOperationQueue* _operationQueue;
}

- (instancetype)initWithProvider:(common::FilePromiseProvider*)provider
                           files:(const proto::clipboard::Event::FileList&)files
                  operationQueue:(NSOperationQueue*)queue
{
    self = [super init];
    if (self)
    {
        _provider = provider;
        _files = files;
        _operationQueue = queue;
    }
    return self;
}

- (NSString*)filePromiseProvider:(NSFilePromiseProvider*)filePromiseProvider
                 fileNameForType:(NSString*)fileType
{
    int index = [filePromiseProvider.userInfo intValue];

    if (index < 0 || index >= _files.file_size())
    {
        LOG(ERROR) << "Invalid file index:" << index;
        return @"unknown";
    }

    const auto& file = _files.file(index);
    QString path = QString::fromStdString(file.path());

    return path.toNSString();
}

- (void)filePromiseProvider:(NSFilePromiseProvider*)filePromiseProvider
          writePromiseToURL:(NSURL*)url
          completionHandler:(void (^)(NSError* _Nullable))completionHandler
{
    int index = [filePromiseProvider.userInfo intValue];

    if (index < 0 || index >= _files.file_size())
    {
        LOG(ERROR) << "Invalid file index:" << index;
        NSError* error = [NSError errorWithDomain:@"com.aspia.clipboard"
                                             code:-1
                                         userInfo:@{ NSLocalizedDescriptionKey: @"Invalid file index" }];
        completionHandler(error);
        return;
    }

    const auto& file = _files.file(index);
    QString dest_path = QString::fromNSString([url path]);

    LOG(INFO) << "File promise write requested for index:" << index
              << "path:" << QString::fromStdString(file.path())
              << "dest:" << dest_path;

    // Ensure parent directories exist (for nested paths like "dir/subdir/file.txt").
    QDir().mkpath(QFileInfo(dest_path).absolutePath());

    if (file.is_dir())
    {
        // Directories have no content — just create the directory.
        QDir().mkpath(dest_path);
        completionHandler(nil);
        return;
    }

    auto* writer = new common::FilePromiseWriter(file.file_size());

    // Notify ClipboardMac that data is needed for this file.
    // This call crosses from the NSOperationQueue thread to the Qt thread.
    _provider->onFileRequested(index, writer);

    // Block this thread until all data is received and written.
    bool success = writer->writeToDestination(dest_path);
    delete writer;

    if (success)
    {
        completionHandler(nil);
    }
    else
    {
        NSError* error = [NSError errorWithDomain:@"com.aspia.clipboard"
                                             code:-2
                                         userInfo:@{ NSLocalizedDescriptionKey: @"File transfer failed" }];
        completionHandler(error);
    }
}

- (NSOperationQueue*)operationQueueForFilePromiseProvider:(NSFilePromiseProvider*)filePromiseProvider
{
    return _operationQueue;
}

@end

// ---------------------------------------------------------------------------
// FilePromiseProvider C++ implementation.
// ---------------------------------------------------------------------------

namespace common {

//--------------------------------------------------------------------------------------------------
FilePromiseProvider::FilePromiseProvider(const proto::clipboard::Event::FileList& files,
                                         FileDataRequestCallback callback)
    : callback_(std::move(callback)),
      files_(files)
{
    operation_queue_ = [[NSOperationQueue alloc] init];
    [operation_queue_ setMaxConcurrentOperationCount:1];

    delegate_ = [[FilePromiseDelegate alloc] initWithProvider:this
                                                        files:files_
                                               operationQueue:operation_queue_];

    NSMutableArray* array = [NSMutableArray array];

    for (int i = 0; i < files_.file_size(); ++i)
    {
        const auto& file = files_.file(i);

        NSFilePromiseProvider* provider = [[NSFilePromiseProvider alloc]
            initWithFileType:UTTypeData.identifier
                    delegate:delegate_];
        provider.userInfo = @(i);
        [array addObject:provider];
    }

    providers_ = [array copy];
}

//--------------------------------------------------------------------------------------------------
FilePromiseProvider::~FilePromiseProvider()
{
    [operation_queue_ cancelAllOperations];
    [operation_queue_ waitUntilAllOperationsAreFinished];
}

//--------------------------------------------------------------------------------------------------
// static
FilePromiseProvider* FilePromiseProvider::create(const proto::clipboard::Event::FileList& files,
                                                  FileDataRequestCallback callback)
{
    if (files.file_size() == 0)
    {
        LOG(ERROR) << "Empty file list";
        return nullptr;
    }

    return new FilePromiseProvider(files, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
NSArray* FilePromiseProvider::providers() const
{
    return providers_;
}

//--------------------------------------------------------------------------------------------------
void FilePromiseProvider::onFileRequested(int file_index, FilePromiseWriter* writer)
{
    if (callback_)
        callback_(file_index, writer);
}

} // namespace common
