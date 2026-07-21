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

#include "common/clipboard_win.h"

#include <QBuffer>
#include <QImage>
#include <QStack>

#include <comdef.h>
#include <ole2.h>
#include <shellapi.h>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/win/message_window.h"
#include "base/win/scoped_clipboard.h"
#include "base/win/scoped_hglobal.h"
#include "base/win/scoped_object.h"
#include "common/win/file_object.h"
#include "common/win/file_stream.h"

namespace {

//--------------------------------------------------------------------------------------------------
int lastSeparator(const QString& path)
{
    int pos = path.lastIndexOf('\\');
    return (pos >= 0) ? pos : path.lastIndexOf('/');
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

        while (i < len && result[i].toLower() == paths[n][i].toLower())
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
bool isDirectory(const QString& path)
{
    DWORD attributes = GetFileAttributesW(qUtf16Printable(path));
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return false;

    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

//--------------------------------------------------------------------------------------------------
qint64 fileTimeToUnixTime(const FILETIME& file_time)
{
    ULARGE_INTEGER ull;

    ull.LowPart = file_time.dwLowDateTime;
    ull.HighPart = file_time.dwHighDateTime;

    return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

//--------------------------------------------------------------------------------------------------
bool fileInfo(const QString& path, FileInfo* file_info)
{
    if (path.isEmpty() || !file_info)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    file_info->is_directory = isDirectory(path);

    const DWORD flags = file_info->is_directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;

    ScopedHandle file(CreateFileW(qUtf16Printable(path), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, flags, nullptr));
    if (!file.isValid())
    {
        PLOG(WARNING) << "Unable to open file" << path;
        return false;
    }

    if (!file_info->is_directory)
    {
        LARGE_INTEGER size;
        if (!GetFileSizeEx(file, &size))
        {
            LOG(WARNING) << "Unable to get file size" << path;
            return false;
        }

        file_info->file_size = size.QuadPart;
    }

    FILETIME filetime_create;
    FILETIME filetime_access;
    FILETIME filetime_write;

    if (!GetFileTime(file, &filetime_create, &filetime_access, &filetime_write))
    {
        PLOG(WARNING) << "Unable to get file time" << path;
        return false;
    }

    file_info->create_time = fileTimeToUnixTime(filetime_create);
    file_info->access_time = fileTimeToUnixTime(filetime_access);
    file_info->modify_time = fileTimeToUnixTime(filetime_write);
    return true;
}

//--------------------------------------------------------------------------------------------------
void addDirectoryContent(const QString& path, QVector<LocalFileEntry>* entries)
{
    QStack<QString> stack;
    stack.push(path);

    while (!stack.isEmpty())
    {
        QString current_path = stack.top();
        stack.pop();

        QString search_path = current_path + "\\*";

        WIN32_FIND_DATAW find_data;
        HANDLE find_handle = FindFirstFileExW(qUtf16Printable(search_path),
                                              FindExInfoBasic, // Omit short name.
                                              &find_data,
                                              FindExSearchNameMatch,
                                              nullptr,
                                              FIND_FIRST_EX_LARGE_FETCH);
        if (find_handle == INVALID_HANDLE_VALUE)
            continue;

        do
        {
            QString name = QString::fromWCharArray(find_data.cFileName);

            if (name == "." || name == "..")
                continue;

            QString full_path = current_path + '\\' + name;
            bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            LocalFileEntry entry;
            entry.path = full_path;
            entry.is_dir = is_directory;
            entry.create_time = fileTimeToUnixTime(find_data.ftCreationTime);
            entry.access_time = fileTimeToUnixTime(find_data.ftLastAccessTime);
            entry.modify_time = fileTimeToUnixTime(find_data.ftLastWriteTime);

            if (is_directory)
            {
                stack.push(full_path);
            }
            else
            {
                ULARGE_INTEGER size;
                size.HighPart = find_data.nFileSizeHigh;
                size.LowPart = find_data.nFileSizeLow;

                entry.file_size = static_cast<qint64>(size.QuadPart);
            }

            entries->append(entry);
        }
        while (FindNextFileW(find_handle, &find_data));

        FindClose(find_handle);
    }
}

//--------------------------------------------------------------------------------------------------
// Registered "HTML Format" clipboard format (CF_HTML).
UINT htmlClipboardFormat()
{
    static const UINT format = RegisterClipboardFormatW(L"HTML Format");
    return format;
}

//--------------------------------------------------------------------------------------------------
// Wraps a UTF-8 HTML fragment into the CF_HTML layout: a header with decimal byte offsets followed
// by an HTML document with the fragment between the StartFragment/EndFragment markers.
QByteArray packHtmlFragment(const QByteArray& fragment)
{
    static const char kHeaderFormat[] =
        "Version:0.9\r\n"
        "StartHTML:%010d\r\n"
        "EndHTML:%010d\r\n"
        "StartFragment:%010d\r\n"
        "EndFragment:%010d\r\n";
    static const char kPrefix[] = "<html>\r\n<body>\r\n<!--StartFragment-->";
    static const char kSuffix[] = "<!--EndFragment-->\r\n</body>\r\n</html>";

    // Each %010d expands to exactly 10 characters, so the header size does not depend on the
    // offset values.
    const int header_size = static_cast<int>(qstrlen(kHeaderFormat)) - 4 * 5 + 4 * 10;

    const int start_html = header_size;
    const int start_fragment = start_html + static_cast<int>(qstrlen(kPrefix));
    const int end_fragment = start_fragment + static_cast<int>(fragment.size());
    const int end_html = end_fragment + static_cast<int>(qstrlen(kSuffix));

    QByteArray result =
        QString::asprintf(kHeaderFormat, start_html, end_html, start_fragment, end_fragment).toLatin1();
    result += kPrefix;
    result += fragment;
    result += kSuffix;
    return result;
}

//--------------------------------------------------------------------------------------------------
// Extracts the UTF-8 HTML fragment from CF_HTML data. The offsets come from another application
// and are validated before use. Returns an empty array on malformed input.
QByteArray unpackHtmlFragment(const QByteArray& data)
{
    auto offset_value = [&data](const char* key) -> qsizetype
    {
        qsizetype pos = data.indexOf(key);
        if (pos < 0)
            return -1;

        pos += qstrlen(key);
        qsizetype end = pos;
        while (end < data.size() && data[end] >= '0' && data[end] <= '9')
            ++end;
        if (end == pos)
            return -1;

        bool ok = false;
        qsizetype value = data.mid(pos, end - pos).toLongLong(&ok);
        return ok ? value : -1;
    };

    qsizetype start = offset_value("StartFragment:");
    qsizetype end = offset_value("EndFragment:");

    if (start < 0 || end < start || end > data.size())
    {
        LOG(WARNING) << "Malformed CF_HTML data";
        return QByteArray();
    }

    return data.mid(start, end - start);
}

//--------------------------------------------------------------------------------------------------
// Registered "PNG" clipboard format. Filled by browsers and editors alongside CF_DIB; carries the
// image losslessly and is used for pass-through without recoding.
UINT pngClipboardFormat()
{
    static const UINT format = RegisterClipboardFormatW(L"PNG");
    return format;
}

//--------------------------------------------------------------------------------------------------
// Registered RTF clipboard format; the payload is passed through unchanged.
UINT rtfClipboardFormat()
{
    static const UINT format = RegisterClipboardFormatW(L"Rich Text Format");
    return format;
}

//--------------------------------------------------------------------------------------------------
// Registered CSV clipboard format (spreadsheet cells); the payload is in the ANSI code page.
UINT csvClipboardFormat()
{
    static const UINT format = RegisterClipboardFormatW(L"Csv");
    return format;
}

//--------------------------------------------------------------------------------------------------
// Registered SVG clipboard format (vector editors); the payload is passed through unchanged.
UINT svgClipboardFormat()
{
    static const UINT format = RegisterClipboardFormatW(L"image/svg+xml");
    return format;
}

//--------------------------------------------------------------------------------------------------
// Converts CF_DIB clipboard data to PNG. The DIB is wrapped into a BMP file layout so that the
// QImage decoder handles all the header/palette/bitfields variants (the data comes from another
// application and is validated by the decoder).
QByteArray dibToPng(const QByteArray& dib)
{
    if (dib.size() < static_cast<qsizetype>(sizeof(BITMAPINFOHEADER)))
        return QByteArray();

    const BITMAPINFOHEADER* header = reinterpret_cast<const BITMAPINFOHEADER*>(dib.constData());
    if (header->biSize < sizeof(BITMAPINFOHEADER) ||
        static_cast<qsizetype>(header->biSize) > dib.size())
    {
        LOG(WARNING) << "Malformed DIB header";
        return QByteArray();
    }

    // The pixel data offset accounts for the color table (mandatory at <= 8 bpp, optional
    // otherwise) and the extra bitfields masks of a plain BITMAPINFOHEADER.
    quint32 color_table_entries = header->biClrUsed;
    if (header->biBitCount <= 8 && !color_table_entries)
        color_table_entries = 1u << header->biBitCount;

    quint32 masks_size = 0;
    if (header->biCompression == BI_BITFIELDS && header->biSize == sizeof(BITMAPINFOHEADER))
        masks_size = 3 * sizeof(DWORD);

    // The offset is computed in 64 bits and bounds-checked: a hostile biClrUsed would otherwise
    // overflow the 32-bit field and point the decoder at a bogus position.
    const quint64 pixel_offset = sizeof(BITMAPFILEHEADER) + header->biSize + masks_size +
        static_cast<quint64>(color_table_entries) * sizeof(RGBQUAD);
    if (pixel_offset >= sizeof(BITMAPFILEHEADER) + static_cast<quint64>(dib.size()))
    {
        LOG(WARNING) << "Malformed DIB header";
        return QByteArray();
    }

    BITMAPFILEHEADER file_header = {};
    file_header.bfType = 0x4D42; // "BM"
    file_header.bfSize = static_cast<DWORD>(sizeof(file_header) + dib.size());
    file_header.bfOffBits = static_cast<DWORD>(pixel_offset);

    QByteArray bmp;
    bmp.reserve(static_cast<qsizetype>(sizeof(file_header)) + dib.size());
    bmp.append(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
    bmp.append(dib);

    QImage image = QImage::fromData(bmp, "BMP");
    if (image.isNull())
    {
        LOG(WARNING) << "Unable to decode DIB image";
        return QByteArray();
    }

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);

    if (!image.save(&buffer, "PNG"))
    {
        LOG(WARNING) << "Unable to encode PNG image";
        return QByteArray();
    }

    return png;
}

//--------------------------------------------------------------------------------------------------
// Converts PNG data to a 32-bit bottom-up CF_DIB.
QByteArray pngToDib(const QByteArray& png)
{
    // The decoder's default allocation limit (256 MB, up to 8192x8192 ARGB32) is kept as a bomb
    // guard: a PNG under the event size cap that decodes past it is synthetic. Such an image is
    // still available on the clipboard through the raw "PNG" format.
    QImage image = QImage::fromData(png, "PNG");
    if (image.isNull())
    {
        LOG(WARNING) << "Unable to decode PNG image";
        return QByteArray();
    }

    image = image.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull())
        return QByteArray();

    const int width = image.width();
    const int height = image.height();
    const quint32 stride = static_cast<quint32>(width) * 4;

    BITMAPINFOHEADER header = {};
    header.biSize = sizeof(header);
    header.biWidth = width;
    header.biHeight = height; // Bottom-up DIB.
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = stride * static_cast<quint32>(height);

    QByteArray dib;
    dib.reserve(static_cast<qsizetype>(sizeof(header)) + header.biSizeImage);
    dib.append(reinterpret_cast<const char*>(&header), sizeof(header));

    // The ARGB32 memory layout on little-endian matches the BGRA byte order of a 32-bit DIB; rows
    // are appended in reverse for the bottom-up layout.
    for (int y = height - 1; y >= 0; --y)
        dib.append(reinterpret_cast<const char*>(image.constScanLine(y)), stride);

    return dib;
}

//--------------------------------------------------------------------------------------------------
// Reads a clipboard format as bytes. |null_terminated| cuts the data at the first null byte:
// text-like formats are often stored with trailing padding after the terminator.
QByteArray clipboardBytes(ScopedClipboard& clipboard, UINT format, bool null_terminated)
{
    HGLOBAL handle = clipboard.data(format);
    if (!handle)
        return QByteArray();

    ScopedHGLOBAL<char> lock(handle);
    if (!lock.get())
    {
        PLOG(ERROR) << "Couldn't lock clipboard data";
        return QByteArray();
    }

    size_t size = lock.size();
    if (null_terminated)
        size = qstrnlen(lock.get(), size);

    return QByteArray(lock.get(), static_cast<qsizetype>(size));
}

//--------------------------------------------------------------------------------------------------
// Copies |data| into a new movable HGLOBAL. Returns null on failure.
HGLOBAL globalAllocData(const void* data, size_t size)
{
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!handle)
    {
        PLOG(ERROR) << "GlobalAlloc failed";
        return nullptr;
    }

    void* locked = GlobalLock(handle);
    if (!locked)
    {
        PLOG(ERROR) << "GlobalLock failed";
        GlobalFree(handle);
        return nullptr;
    }

    memcpy(locked, data, size);
    GlobalUnlock(handle);
    return handle;
}

//--------------------------------------------------------------------------------------------------
// Sets clipboard format markers that exclude the current clipboard content from Windows
// clipboard history (Win+V) and Cloud Clipboard sync. Must be called while the clipboard is open.
void excludeFromClipboardHistory(ScopedClipboard& clipboard)
{
    // Tells clipboard monitors/filters (including the OS history service) to skip this content.
    static const UINT exclude_format =
        RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");

    // Documented opt-out flags for clipboard history and cloud clipboard.
    static const UINT history_format = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    static const UINT cloud_format = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");

    auto setFlag = [&clipboard](UINT format, DWORD value)
    {
        if (!format)
            return;

        HGLOBAL handle = globalAllocData(&value, sizeof(value));
        if (handle)
            clipboard.setData(format, handle);
    };

    setFlag(exclude_format, 0);
    setFlag(history_format, 0);
    setFlag(cloud_format, 0);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClipboardWin::ClipboardWin(QObject* parent)
    : Clipboard(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClipboardWin::~ClipboardWin()
{
    LOG(INFO) << "Dtor";

    if (!window_)
    {
        LOG(ERROR) << "Window not created";
        return;
    }

    // Release the streams and the OLE clipboard object before uninitializing OLE.
    releaseFileTransferState();

    RemoveClipboardFormatListener(window_->hwnd());
    window_.reset();

    OleUninitialize();
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::addFileData(int file_index, const QByteArray& data, bool is_last)
{
    auto it = active_streams_.find(file_index);
    if (it == active_streams_.end())
    {
        LOG(ERROR) << "No active stream for file_index:" << file_index;
        return;
    }

    FileStream* stream = it.value();

    if (!data.isEmpty())
        stream->addData(data);

    if (is_last)
    {
        stream->terminate();
        stream->Release();
        active_streams_.erase(it);
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::init()
{
    if (window_)
    {
        LOG(ERROR) << "Window already created";
        return;
    }

    window_ = std::make_unique<MessageWindow>();

    if (!window_->create(std::bind(&ClipboardWin::onMessage,
                                   this,
                                   std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3, std::placeholders::_4)))
    {
        LOG(ERROR) << "Couldn't create clipboard window";
        return;
    }

    if (!AddClipboardFormatListener(window_->hwnd()))
    {
        PLOG(ERROR) << "AddClipboardFormatListener failed";
        return;
    }

    _com_error error = OleInitialize(nullptr);
    if (FAILED(error.Error()))
    {
        PLOG(ERROR) << "OleInitialize failed";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::setData(const proto::clipboard::Event& event)
{
    if (!window_)
    {
        LOG(ERROR) << "Window not created";
        return;
    }

    if (!event.format_size())
    {
        clearClipboardContent();
        return;
    }

    // A file list cannot be combined with other formats in the OLE clipboard, so it takes priority.
    const proto::clipboard::Event::Format* file_format = findFormat(event, kMimeTypeFileList);
    if (file_format)
    {
        setDataFiles(QByteArray::fromStdString(file_format->data()));
        return;
    }

    setDataContent(event);
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::onClipboardUpdate()
{
    // Skip updates caused by our own injection: while we own the clipboard the change came from
    // setDataContent(), so re-capturing it would echo it back to the remote side.
    if (GetClipboardOwner() == window_->hwnd())
        return;

    if (IsClipboardFormatAvailable(CF_HDROP))
    {
        onClipboardFiles();
        return;
    }

    if (IsClipboardFormatAvailable(CF_UNICODETEXT) ||
        IsClipboardFormatAvailable(htmlClipboardFormat()) ||
        IsClipboardFormatAvailable(rtfClipboardFormat()) ||
        IsClipboardFormatAvailable(csvClipboardFormat()) ||
        IsClipboardFormatAvailable(pngClipboardFormat()) ||
        IsClipboardFormatAvailable(svgClipboardFormat()) ||
        IsClipboardFormatAvailable(CF_DIB))
    {
        onClipboardData();
        return;
    }

    // Unhandled clipboard type.
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::onClipboardData()
{
    QString text;
    QString csv;
    QByteArray html;
    QByteArray rtf;
    QByteArray png;
    QByteArray svg;
    QByteArray dib;

    // Add a scope, so that we keep the clipboard open for as short a time as possible.
    {
        ScopedClipboard clipboard;

        if (!clipboard.init(window_->hwnd()))
        {
            PLOG(ERROR) << "Couldn't open the clipboard";
            return;
        }

        HGLOBAL text_global = clipboard.data(CF_UNICODETEXT);
        if (text_global)
        {
            ScopedHGLOBAL<wchar_t> text_lock(text_global);
            if (text_lock.get())
            {
                // The allocation is not guaranteed to be null-terminated, so bound the read.
                text = QString::fromWCharArray(text_lock.get(),
                    static_cast<qsizetype>(wcsnlen(text_lock.get(),
                                                   text_lock.size() / sizeof(wchar_t))));
            }
            else
            {
                PLOG(ERROR) << "Couldn't lock clipboard data";
            }
        }

        // CF_HTML data is UTF-8; the allocation may be larger than the payload, so cut at the
        // first null terminator.
        html = unpackHtmlFragment(clipboardBytes(clipboard, htmlClipboardFormat(), true));

        rtf = clipboardBytes(clipboard, rtfClipboardFormat(), true);

        // The CSV payload is in the ANSI code page; the wire format is UTF-8.
        csv = QString::fromLocal8Bit(clipboardBytes(clipboard, csvClipboardFormat(), true));

        svg = clipboardBytes(clipboard, svgClipboardFormat(), true);

        // The registered "PNG" format is passed through as is; CF_DIB is only copied here and
        // recoded after the clipboard is closed.
        png = clipboardBytes(clipboard, pngClipboardFormat(), false);
        if (png.isEmpty())
            dib = clipboardBytes(clipboard, CF_DIB, false);
    }

    if (png.isEmpty() && !dib.isEmpty())
        png = dibToPng(dib);

    proto::clipboard::Event event;

    if (!text.isEmpty())
        text.replace("\r\n", "\n");

    addFormat(&event, kMimeTypeTextUtf8, text.toUtf8());
    addFormat(&event, kMimeTypeTextHtml, html);
    addFormat(&event, kMimeTypeTextRtf, rtf);
    addFormat(&event, kMimeTypeTextCsv, csv.toUtf8());
    addFormat(&event, kMimeTypeImagePng, png);
    addFormat(&event, kMimeTypeImageSvg, svg);

    onData(std::move(event));
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::onClipboardFiles()
{
    QStringList files;

    // Add a scope, so that we keep the clipboard open for as short a time as possible.
    {
        ScopedClipboard clipboard;

        if (!clipboard.init(window_->hwnd()))
        {
            PLOG(ERROR) << "Couldn't open the clipboard";
            return;
        }

        HDROP drop_handle = reinterpret_cast<HDROP>(clipboard.data(CF_HDROP));
        if (!drop_handle)
        {
            PLOG(ERROR) << "Couldn't get data from the clipboard";
            return;
        }

        UINT file_count = DragQueryFileW(drop_handle, 0xFFFFFFFF, nullptr, 0);

        for (UINT i = 0; i < file_count; ++i)
        {
            UINT length = DragQueryFileW(drop_handle, i, nullptr, 0); // Length without \0.
            if (!length)
            {
                PLOG(ERROR) << "DragQueryFileW failed";
                continue;
            }

            std::wstring file_path(length + 1, L'\0');

            if (!DragQueryFileW(drop_handle, i, file_path.data(), length + 1))
            {
                PLOG(ERROR) << "DragQueryFileW failed";
                continue;
            }

            files.append(QString::fromWCharArray(file_path.c_str(), wcslen(file_path.c_str())));
        }
    }

    QVector<LocalFileEntry> entries;
    QString common_parent = commonParentDir(files);

    for (const auto& path : std::as_const(files))
    {
        FileInfo file_info;
        if (!fileInfo(path, &file_info))
            continue;

        LocalFileEntry entry;
        entry.path = path;
        entry.is_dir = file_info.is_directory;
        entry.file_size = file_info.file_size;
        entry.create_time = file_info.create_time;
        entry.access_time = file_info.access_time;
        entry.modify_time = file_info.modify_time;
        entries.append(entry);

        // When copying, only top-level paths are copied to the clipboard. If the directory has
        // nested elements, they must be added separately.
        if (file_info.is_directory)
            addDirectoryContent(path, &entries);
    }

    // Store local file list for file data requests.
    emit sig_localFileListChanged(entries);

    // Build proto with relative paths and send clipboard event through normal clipboard path.
    proto::clipboard::Event::FileList file_list;

    for (const auto& entry : std::as_const(entries))
    {
        proto::clipboard::Event::FileList::File* file = file_list.add_file();

        QString relative_path = entry.path;
        if (!common_parent.isEmpty() &&
            relative_path.startsWith(common_parent, Qt::CaseInsensitive))
        {
            relative_path = relative_path.mid(common_parent.size());
        }

        // Normalize path separators to '/' for cross-platform compatibility.
        relative_path.replace('\\', '/');

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
void ClipboardWin::setDataContent(const proto::clipboard::Event& event)
{
    const proto::clipboard::Event::Format* text_format = findFormat(event, kMimeTypeTextUtf8);
    const proto::clipboard::Event::Format* html_format = findFormat(event, kMimeTypeTextHtml);
    const proto::clipboard::Event::Format* rtf_format = findFormat(event, kMimeTypeTextRtf);
    const proto::clipboard::Event::Format* csv_format = findFormat(event, kMimeTypeTextCsv);
    const proto::clipboard::Event::Format* png_format = findFormat(event, kMimeTypeImagePng);
    const proto::clipboard::Event::Format* svg_format = findFormat(event, kMimeTypeImageSvg);

    // A read-only view of the payload without copying it. The protobuf payload is null-terminated
    // at data()[size()], so reading the terminator byte past the view below is safe.
    auto raw_data = [](const proto::clipboard::Event::Format* format)
    {
        return QByteArray::fromRawData(format->data().data(),
                                       static_cast<qsizetype>(format->data().size()));
    };

    // Prepare every payload before opening the clipboard, so that the open/close bracket contains
    // only the allocation and SetClipboardData calls: while the clipboard is held open, other
    // applications fail to access it.
    QString wide_text;
    if (text_format && !text_format->data().empty())
    {
        wide_text = QString::fromUtf8(raw_data(text_format));
        wide_text.replace("\n", "\r\n");
    }

    // CF_HTML data is UTF-8 and null-terminated.
    QByteArray packed_html;
    if (html_format)
        packed_html = packHtmlFragment(raw_data(html_format));

    // The wire format is UTF-8; the clipboard payload is in the ANSI code page.
    QByteArray ansi_csv;
    if (csv_format)
        ansi_csv = QString::fromUtf8(raw_data(csv_format)).toLocal8Bit();

    QByteArray dib;
    if (png_format)
        dib = pngToDib(raw_data(png_format));

    // Nothing convertible to a Windows format: leave the local clipboard untouched. Only an empty
    // event is a clear command, and that path goes through clearClipboardContent().
    const bool has_payload = !wide_text.isEmpty() ||
        !packed_html.isEmpty() || (rtf_format && !rtf_format->data().empty()) ||
        !ansi_csv.isEmpty() || (png_format && !png_format->data().empty()) ||
        (svg_format && !svg_format->data().empty());
    if (!has_payload)
    {
        LOG(WARNING) << "No usable payload in clipboard event";
        return;
    }

    ScopedClipboard clipboard;
    if (!clipboard.init(window_->hwnd()))
    {
        PLOG(ERROR) << "Couldn't open the clipboard";
        return;
    }

    // All formats describe one clipboard change and are set within a single clipboard open.
    clipboard.empty();

    bool has_content = false;

    // Sets |data| plus a null terminator of |terminator_size| bytes as clipboard format |format|.
    auto set_bytes = [&clipboard, &has_content](
        UINT format, const QByteArray& data, size_t terminator_size)
    {
        if (data.isEmpty())
            return;

        HGLOBAL handle =
            globalAllocData(data.constData(), static_cast<size_t>(data.size()) + terminator_size);
        if (handle)
        {
            clipboard.setData(format, handle);
            has_content = true;
        }
    };

    // Sets |text| as a null-terminated UTF-16 clipboard format |format|.
    auto set_wide = [&clipboard, &has_content](UINT format, const QString& text)
    {
        if (text.isEmpty())
            return;

        // utf16() is null-terminated, copying size() + 1 characters includes the terminator.
        HGLOBAL handle = globalAllocData(
            text.utf16(), (static_cast<size_t>(text.size()) + 1) * sizeof(wchar_t));
        if (handle)
        {
            clipboard.setData(format, handle);
            has_content = true;
        }
    };

    set_wide(CF_UNICODETEXT, wide_text);
    set_bytes(htmlClipboardFormat(), packed_html, 1);

    if (rtf_format)
        set_bytes(rtfClipboardFormat(), raw_data(rtf_format), 1);

    set_bytes(csvClipboardFormat(), ansi_csv, 1);

    if (png_format)
    {
        // The original PNG is set alongside CF_DIB: applications that understand it get the image
        // losslessly, and the capture side prefers it so no re-encoding is needed.
        set_bytes(pngClipboardFormat(), raw_data(png_format), 0);
        set_bytes(CF_DIB, dib, 0);
    }

    if (svg_format)
        set_bytes(svgClipboardFormat(), raw_data(svg_format), 1);

    if (!has_content)
        return;

    // Prevent the content (which may contain passwords typed on the remote machine) from being
    // stored in the local clipboard history or synchronized via Cloud Clipboard.
    excludeFromClipboardHistory(clipboard);
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::setDataFiles(const QByteArray& data)
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

    releaseFileTransferState();

    file_object_.reset(FileObject::create(files,
        [this](int file_index, FileStream* stream)
        {
            onFileDataRequested(file_index, stream);
        }));

    if (!file_object_)
    {
        LOG(ERROR) << "Invalid file object";
        return;
    }

    _com_error error = OleSetClipboard(file_object_.get());
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "OleSetClipboard failed:" << error;
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::clearClipboardContent()
{
    // Release any active file streams and our OLE clipboard data object first, otherwise the
    // CF_HDROP / IDataObject content stays attached and a subsequent EmptyClipboard has nothing
    // to remove on the OLE side.
    releaseFileTransferState();

    // Make sure no other process owns the clipboard data.
    OleFlushClipboard();

    ScopedClipboard clipboard;
    if (!clipboard.init(window_->hwnd()))
    {
        PLOG(ERROR) << "Couldn't open the clipboard";
        return;
    }

    clipboard.empty();
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::releaseFileTransferState()
{
    // Terminate and release any active streams.
    for (auto it = active_streams_.begin(); it != active_streams_.end(); ++it)
    {
        it.value()->terminate();
        it.value()->Release();
    }
    active_streams_.clear();

    // Release the OLE clipboard reference to the file object before destroying it. Otherwise,
    // OLE holds a dangling pointer and crashes on access.
    if (file_object_)
    {
        OleSetClipboard(nullptr);
        file_object_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardWin::onFileDataRequested(int file_index, FileStream* stream)
{
    // Clean up any previous stream for this file_index (e.g., from a cancelled transfer).
    auto it = active_streams_.find(file_index);
    if (it != active_streams_.end())
    {
        it.value()->terminate();
        it.value()->Release();
        active_streams_.erase(it);
    }

    stream->AddRef();
    active_streams_[file_index] = stream;
    emit sig_fileDataRequest(file_index);
}

//--------------------------------------------------------------------------------------------------
bool ClipboardWin::onMessage(UINT message, WPARAM /* wParam */, LPARAM /* lParam */, LRESULT& result)
{
    switch (message)
    {
        case WM_CLIPBOARDUPDATE:
            onClipboardUpdate();
            break;

        default:
            return false;
    }

    result = 0;
    return true;
}
