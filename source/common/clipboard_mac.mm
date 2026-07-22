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
#include "base/time_types.h"
#include "common/mac/file_data_provider.h"
#include "common/mac/file_promise_writer.h"

#import <Cocoa/Cocoa.h>

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
struct LocalFileInfo
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
bool fileInfo(const QString& path, LocalFileInfo* file_info)
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

            LocalFileInfo fi;
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
    connect(timer_, &QTimer::timeout, this, &ClipboardMac::onPollTimer);
}

//--------------------------------------------------------------------------------------------------
ClipboardMac::~ClipboardMac()
{
    LOG(INFO) << "Dtor";

    // Terminate any active writers.
    {
        std::scoped_lock lock(writers_mutex_);
        closing_ = true;
        for (auto it = active_writers_.begin(); it != active_writers_.end(); ++it)
            it.value()->terminate();
        active_writers_.clear();
    }

    // Release the file data provider (waits for in-flight download blocks, cleans up temp
    // directory). A block that has not registered its writer yet is turned away by |closing_| in
    // onFileDataRequested(), so the wait cannot hang on a writer nobody would ever terminate.
    file_data_provider_.reset();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::addFileData(int file_index, const QByteArray& data, bool is_last)
{
    std::scoped_lock lock(writers_mutex_);

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

//--------------------------------------------------------------------------------------------------
void ClipboardMac::init()
{
    current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];
    startTimer();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::setData(const proto::clipboard::Event& event)
{
    // Guard: prevent onPollTimer() from reading our own pasteboard writes.
    // NSPasteboard IPC calls (writeObjects:, clearContents) may pump the run loop
    // internally, causing the QTimer to fire mid-write and trigger a re-read
    // that would call provideDataForType: prematurely.
    is_setting_data_ = true;

    // A file list cannot be combined with other formats in the pasteboard, so it takes priority.
    const proto::clipboard::Event::Format* file_format = findFormat(event, kMimeTypeFileList);

    if (file_format)
    {
        setDataFiles(QByteArray::fromStdString(file_format->data()));
    }
    else if (!event.format_size())
    {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        current_change_count_ = [pasteboard changeCount];
    }
    else
    {
        setDataContent(event);
    }

    is_setting_data_ = false;
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::onPollTimer()
{
    // Skip if we are currently writing to the pasteboard (see setData guard).
    if (is_setting_data_)
        return;

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

    onClipboardData();
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::startTimer()
{
    timer_->start(Milliseconds(1000));
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::onClipboardData()
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    proto::clipboard::Event event;

    auto add_string = [&event, pasteboard](const char* mime_type, NSPasteboardType type)
    {
        NSString* value = [pasteboard stringForType:type];
        if (value)
            addFormat(&event, mime_type, QString::fromNSString(value).toUtf8());
    };

    auto add_bytes = [&event, pasteboard](const char* mime_type, NSPasteboardType type)
    {
        NSData* value = [pasteboard dataForType:type];
        if (value)
            addFormat(&event, mime_type, QByteArray::fromNSData(value));
    };

    // stringForType reads only the first pasteboard item; fall back to AppKit's object reading,
    // which materializes a string from any item or bridgeable flavor (multi-item pasteboards may
    // keep the text in a later item).
    NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
    if (!text)
    {
        NSArray* objects = [pasteboard readObjectsForClasses:@[ [NSString class] ] options:nil];
        if ([objects count])
            text = [objects lastObject];
    }

    if (text)
        addFormat(&event, kMimeTypeTextUtf8, QString::fromNSString(text).toUtf8());

    add_string(kMimeTypeTextHtml, NSPasteboardTypeHTML);
    add_bytes(kMimeTypeTextRtf, NSPasteboardTypeRTF);
    add_string(kMimeTypeTextCsv, @"public.comma-separated-values-text");

    // PNG is passed through as is; a TIFF-only pasteboard (screenshots, some editors) is recoded.
    NSData* png = [pasteboard dataForType:NSPasteboardTypePNG];
    if (!png)
    {
        NSData* tiff = [pasteboard dataForType:NSPasteboardTypeTIFF];
        if (tiff)
        {
            NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:tiff];
            if (rep)
                png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        }
    }

    if (png)
        addFormat(&event, kMimeTypeImagePng, QByteArray::fromNSData(png));

    add_bytes(kMimeTypeImageSvg, @"public.svg-image");

    onData(std::move(event));
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
        LocalFileInfo fi;
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

    proto::clipboard::Event event;
    addFormat(&event, kMimeTypeFileList, serialize(file_list));
    onData(std::move(event));
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::setDataContent(const proto::clipboard::Event& event)
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];

    // All formats describe one clipboard change and are published in a single pasteboard item.
    // The pasteboard is cleared only after at least one format is known to be usable: only an
    // empty event is a clear command.
    NSPasteboardItem* item = [[[NSPasteboardItem alloc] init] autorelease];
    bool has_content = false;

    auto set_string = [item, &has_content, &event](const char* mime_type, NSPasteboardType type)
    {
        const proto::clipboard::Event::Format* format = findFormat(event, mime_type);
        if (!format || format->data().empty())
            return;

        [item setString:QString::fromStdString(format->data()).toNSString() forType:type];
        has_content = true;
    };

    auto set_bytes = [item, &has_content, &event](const char* mime_type, NSPasteboardType type)
    {
        const proto::clipboard::Event::Format* format = findFormat(event, mime_type);
        if (!format || format->data().empty())
            return;

        [item setData:QByteArray::fromStdString(format->data()).toNSData() forType:type];
        has_content = true;
    };

    set_string(kMimeTypeTextUtf8, NSPasteboardTypeString);
    set_string(kMimeTypeTextHtml, NSPasteboardTypeHTML);
    set_bytes(kMimeTypeTextRtf, NSPasteboardTypeRTF);
    set_string(kMimeTypeTextCsv, @"public.comma-separated-values-text");
    set_bytes(kMimeTypeImageSvg, @"public.svg-image");

    const proto::clipboard::Event::Format* png_format = findFormat(event, kMimeTypeImagePng);
    if (png_format && !png_format->data().empty())
    {
        NSData* png_data = QByteArray::fromStdString(png_format->data()).toNSData();
        [item setData:png_data forType:NSPasteboardTypePNG];
        has_content = true;

        // A TIFF representation is published alongside for applications that do not accept PNG.
        NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:png_data];
        if (rep)
        {
            NSData* tiff = [rep TIFFRepresentation];
            if (tiff)
                [item setData:tiff forType:NSPasteboardTypeTIFF];
        }
    }

    if (!has_content)
    {
        LOG(WARNING) << "No usable payload in clipboard event";
        return;
    }

    [pasteboard clearContents];
    [pasteboard writeObjects:@[ item ]];
    current_change_count_ = [pasteboard changeCount];
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::setDataFiles(const QByteArray& data)
{
    proto::clipboard::Event::FileList files;

    if (!parse(data, &files))
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
    {
        std::scoped_lock lock(writers_mutex_);
        closing_ = true;
        for (auto it = active_writers_.begin(); it != active_writers_.end(); ++it)
            it.value()->terminate();
        active_writers_.clear();
    }

    // Release the old provider before creating a new one. Its destructor waits for in-flight
    // download blocks; a block that has not registered its writer yet is turned away by
    // |closing_|, so the wait cannot hang.
    file_data_provider_.reset();

    {
        std::scoped_lock lock(writers_mutex_);
        closing_ = false;
    }

    file_data_provider_.reset(FileDataProvider::create(files,
        [this](int file_index, FilePromiseWriter* writer)
        {
            onFileDataRequested(file_index, writer);
        },
        [this](int file_index, FilePromiseWriter* writer)
        {
            onFileDataFinished(file_index, writer);
        }));

    if (!file_data_provider_)
    {
        LOG(ERROR) << "Invalid file data provider";
        return;
    }

    // writeToPasteboard creates empty temp files, registers NSFilePresenters, and
    // writes file URLs to the pasteboard. This is instant - no network transfer.
    // Data is downloaded on-demand when the user actually pastes in Finder
    // (via relinquishPresentedItemToReader:).
    file_data_provider_->writeToPasteboard();
    current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];
}

//--------------------------------------------------------------------------------------------------
void ClipboardMac::onFileDataRequested(int file_index, FilePromiseWriter* writer)
{
    // Called from NSFilePresenter's operation queue (background thread).
    // Protect active_writers_ since addFileData runs on the clipboard thread.
    std::scoped_lock lock(writers_mutex_);

    // The clipboard is tearing down or replacing the provider: nobody would feed or terminate
    // this writer, and the provider destructor would wait on its presenter block forever.
    if (closing_)
    {
        writer->terminate();
        return;
    }

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
void ClipboardMac::onFileDataFinished(int file_index, FilePromiseWriter* writer)
{
    // Called from NSFilePresenter's operation queue right before |writer| is deleted. On a normal
    // completion the entry is already erased by addFileData(); on an error path (destination not
    // writable, write failure) it is still here and would become dangling.
    std::scoped_lock lock(writers_mutex_);

    auto it = active_writers_.find(file_index);
    if (it != active_writers_.end() && it.value() == writer)
        active_writers_.erase(it);
}
