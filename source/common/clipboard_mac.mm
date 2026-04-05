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

#include "common/clipboard_mac.h"

#include <QStack>

#include "base/logging.h"
#include "base/serialization.h"
#include "common/mac/file_promise_provider.h"
#include "common/mac/file_promise_writer.h"

#import <Cocoa/Cocoa.h>

namespace common {

namespace {

//--------------------------------------------------------------------------------------------------
int lastSeparator(const QString& path)
{
    return path.lastIndexOf('/');
}

//--------------------------------------------------------------------------------------------------
QString commonParentDir(const QStringList& paths)
{
    if (paths.isEmpty())
        return QString();

    int sep = lastSeparator(paths.first());
    QString result = (sep >= 0) ? paths.first().left(sep + 1) : QString();

    for (int n = 1; n < paths.size(); ++n)
    {
        int len = std::min(result.size(), paths[n].size());
        int i = 0;

        while (i < len && result[i] == paths[n][i])
            ++i;

        sep = lastSeparator(result.left(i));
        result = (sep >= 0) ? result.left(sep + 1) : QString();

        if (result.isEmpty())
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
struct FileInfo
{
    bool is_directory = false;
    qint64 file_size = 0;
    qint64 create_time = 0;
    qint64 access_time = 0;
    qint64 modify_time = 0;
};

//--------------------------------------------------------------------------------------------------
qint64 nsDateToUnixTime(NSDate* date)
{
    if (!date)
        return 0;
    return static_cast<qint64>([date timeIntervalSince1970]);
}

//--------------------------------------------------------------------------------------------------
bool fileInfo(const QString& path, FileInfo* file_info)
{
    if (path.isEmpty() || !file_info)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    NSFileManager* fm = [NSFileManager defaultManager];
    NSString* ns_path = path.toNSString();

    NSDictionary* attrs = [fm attributesOfItemAtPath:ns_path error:nil];
    if (!attrs)
    {
        LOG(WARNING) << "Unable to get attributes for:" << path;
        return false;
    }

    NSString* file_type = attrs[NSFileType];
    file_info->is_directory = [file_type isEqualToString:NSFileTypeDirectory];

    if (!file_info->is_directory)
        file_info->file_size = [attrs[NSFileSize] longLongValue];

    file_info->create_time = nsDateToUnixTime(attrs[NSFileCreationDate]);
    file_info->modify_time = nsDateToUnixTime(attrs[NSFileModificationDate]);
    file_info->access_time = file_info->modify_time;

    return true;
}

//--------------------------------------------------------------------------------------------------
void addDirectoryContent(const QString& path, QVector<LocalFileEntry>* entries)
{
    QStack<QString> stack;
    stack.push(path);

    NSFileManager* fm = [NSFileManager defaultManager];

    while (!stack.isEmpty())
    {
        QString current_path = stack.top();
        stack.pop();

        NSString* ns_path = current_path.toNSString();
        NSArray<NSString*>* contents = [fm contentsOfDirectoryAtPath:ns_path error:nil];
        if (!contents)
            continue;

        for (NSString* name in contents)
        {
            if ([name hasPrefix:@"."])
                continue;

            QString full_path = current_path + '/' + QString::fromNSString(name);

            FileInfo fi;
            if (!fileInfo(full_path, &fi))
                continue;

            LocalFileEntry entry;
            entry.path = full_path;
            entry.is_dir = fi.is_directory;
            entry.file_size = fi.file_size;
            entry.create_time = fi.create_time;
            entry.access_time = fi.access_time;
            entry.modify_time = fi.modify_time;
            entries->append(entry);

            if (fi.is_directory)
                stack.push(full_path);
        }
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClipboardMac::ClipboardMac(QObject* parent)
    : Clipboard(parent),
      timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";
    connect(timer_, &QTimer::timeout, this, &ClipboardMac::checkForChanges);
}

//--------------------------------------------------------------------------------------------------
ClipboardMac::~ClipboardMac()
{
    LOG(INFO) << "Dtor";

    // Terminate any active writers.
    for (auto it = active_writers_.begin(); it != active_writers_.end(); ++it)
        it.value()->terminate();
    active_writers_.clear();

    // Release the file promise provider (cancels pending operations).
    file_promise_provider_.reset();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::init()
{
    current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];
    startTimer();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::setData(const QString& mime_type, const QByteArray& data)
{
    if (mime_type == kMimeTypeTextUtf8)
    {
        setDataText(data);
    }
    else if (mime_type == kMimeTypeFileList)
    {
        setDataFiles(data);
    }
    else
    {
        LOG(WARNING) << "Unhandled mime type:" << mime_type;
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::startTimer()
{
    timer_->start(std::chrono::milliseconds(1000));
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::checkForChanges()
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSInteger change_count = [pasteboard changeCount];

    if (change_count == current_change_count_)
        return;

    current_change_count_ = change_count;

    // File URLs take priority over text.
    if ([[pasteboard types] containsObject:NSPasteboardTypeFileURL])
    {
        onClipboardFiles();
        return;
    }

    NSArray* objects = [pasteboard readObjectsForClasses:@[ [NSString class] ] options:nil];
    if ([objects count])
        onClipboardText();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::onClipboardText()
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSArray* objects = [pasteboard readObjectsForClasses:@[ [NSString class] ] options:nil];
    if (![objects count])
        return;

    QString data = QString::fromNSString([objects lastObject]);
    if (!data.isEmpty())
        onData(kMimeTypeTextUtf8, data.toUtf8());
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::onClipboardFiles()
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];

    NSArray<NSURL*>* urls = [pasteboard readObjectsForClasses:@[ [NSURL class] ]
                                                      options:@{ NSPasteboardURLReadingFileURLsOnlyKey: @YES }];
    if (![urls count])
        return;

    QStringList files;
    for (NSURL* url in urls)
    {
        if (![url isFileURL])
            continue;

        files.append(QString::fromNSString([url path]));
    }

    if (files.isEmpty())
        return;

    QVector<LocalFileEntry> entries;
    QString common_parent = commonParentDir(files);

    for (const auto& path : std::as_const(files))
    {
        FileInfo fi;
        if (!fileInfo(path, &fi))
            continue;

        LocalFileEntry entry;
        entry.path = path;
        entry.is_dir = fi.is_directory;
        entry.file_size = fi.file_size;
        entry.create_time = fi.create_time;
        entry.access_time = fi.access_time;
        entry.modify_time = fi.modify_time;
        entries.append(entry);

        if (fi.is_directory)
            addDirectoryContent(path, &entries);
    }

    emit sig_localFileListChanged(entries);

    proto::clipboard::Event::FileList file_list;

    for (const auto& entry : std::as_const(entries))
    {
        proto::clipboard::Event::FileList::File* file = file_list.add_file();

        QString relative_path = entry.path;
        if (!common_parent.isEmpty() &&
            relative_path.startsWith(common_parent, Qt::CaseSensitive))
        {
            relative_path = relative_path.mid(common_parent.size());
        }

        file->set_path(relative_path.toStdString());
        file->set_is_dir(entry.is_dir);
        file->set_file_size(entry.file_size);
        file->set_create_time(entry.create_time);
        file->set_access_time(entry.access_time);
        file->set_modify_time(entry.modify_time);
    }

    onData(kMimeTypeFileList, base::serialize(file_list));
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::setDataText(const QByteArray& data)
{
    NSString* text = QString::fromUtf8(data).toNSString();
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[ text ]];

    current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::setDataFiles(const QByteArray& data)
{
    proto::clipboard::Event::FileList files;

    if (!base::parse(data, &files))
    {
        LOG(ERROR) << "Unable to parse file list";
        return;
    }

    if (!files.file_size())
    {
        LOG(ERROR) << "Empty file list";
        return;
    }

    // Terminate any active writers from previous provider.
    for (auto it = active_writers_.begin(); it != active_writers_.end(); ++it)
        it.value()->terminate();
    active_writers_.clear();

    // Release the old provider before creating a new one.
    file_promise_provider_.reset();

    file_promise_provider_.reset(FilePromiseProvider::create(files,
        [this](int file_index, FilePromiseWriter* writer)
        {
            onFileDataRequested(file_index, writer);
        }));

    if (!file_promise_provider_)
    {
        LOG(ERROR) << "Invalid file promise provider";
        return;
    }

    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:file_promise_provider_->providers()];

    current_change_count_ = [pasteboard changeCount];
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::onFileDataRequested(int file_index, FilePromiseWriter* writer)
{
    // Clean up any previous writer for this file_index.
    auto it = active_writers_.find(file_index);
    if (it != active_writers_.end())
    {
        it.value()->terminate();
        active_writers_.erase(it);
    }

    active_writers_[file_index] = writer;
    emit sig_fileDataRequest(file_index);
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::addFileData(int file_index, const QByteArray& data, bool is_last)
{
    auto it = active_writers_.find(file_index);
    if (it == active_writers_.end())
    {
        LOG(ERROR) << "No active writer for file_index:" << file_index;
        return;
    }

    FilePromiseWriter* writer = it.value();
    writer->addData(data, is_last);

    if (is_last)
        active_writers_.erase(it);
}

} // namespace common
