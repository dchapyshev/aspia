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

#include "host/win/msi_package.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>
#include <qt_windows.h>

#include <fci.h>
#include <msi.h>
#include <msidefs.h>
#include <msiquery.h>
#include <wtypes.h>

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <string>

#include "base/logging.h"
#include "base/system_error.h"
#include "host/settings_util.h"

namespace {

// Must match the UpgradeCode of the host package in installer/windows/host.wxs.
const wchar_t kUpgradeCode[] = L"{B460F717-1546-4FFC-9EDE-B21FD07E07CB}";

const char kExecutableFile[] = "aspia_host.exe";
const char kImportAction[] = "ImportSettings";

// Rows added to the installed package. The component code is the same in every exported package.
const char kSettingsComponent[] = "Settings";
const char kSettingsComponentId[] = "{89507617-EC36-4B86-A547-5E3AFE91398F}";
const char kSettingsFileName[] = "aspiahst.jsn|aspia_host.json";
const char kSettingsFileSddl[] = "D:P(A;;FA;;;SY)(A;;FA;;;BA)";

// The settings file is the only file of this cabinet. Inside the cabinet it is named by its key in the
// File table of the package.
const char kSettingsFile[] = "aspia_host.json";
const char kSettingsCabinet[] = "config.cab";

// The host does not link with msi.dll and cabinet.dll: their functions are loaded on first use.
using PFN_MsiCloseHandle = decltype(&::MsiCloseHandle);
using PFN_MsiCreateRecord = decltype(&::MsiCreateRecord);
using PFN_MsiDatabaseCommit = decltype(&::MsiDatabaseCommit);
using PFN_MsiDatabaseIsTablePersistentW = decltype(&::MsiDatabaseIsTablePersistentW);
using PFN_MsiDatabaseOpenViewW = decltype(&::MsiDatabaseOpenViewW);
using PFN_MsiEnumRelatedProductsW = decltype(&::MsiEnumRelatedProductsW);
using PFN_MsiGetProductInfoW = decltype(&::MsiGetProductInfoW);
using PFN_MsiGetSummaryInformationW = decltype(&::MsiGetSummaryInformationW);
using PFN_MsiOpenDatabaseW = decltype(&::MsiOpenDatabaseW);
using PFN_MsiRecordGetInteger = decltype(&::MsiRecordGetInteger);
using PFN_MsiRecordGetStringW = decltype(&::MsiRecordGetStringW);
using PFN_MsiRecordSetInteger = decltype(&::MsiRecordSetInteger);
using PFN_MsiRecordSetStreamW = decltype(&::MsiRecordSetStreamW);
using PFN_MsiRecordSetStringW = decltype(&::MsiRecordSetStringW);
using PFN_MsiSummaryInfoPersist = decltype(&::MsiSummaryInfoPersist);
using PFN_MsiSummaryInfoSetPropertyW = decltype(&::MsiSummaryInfoSetPropertyW);
using PFN_MsiViewExecute = decltype(&::MsiViewExecute);
using PFN_MsiViewFetch = decltype(&::MsiViewFetch);
using PFN_MsiViewModify = decltype(&::MsiViewModify);
using PFN_FCIAddFile = decltype(&::FCIAddFile);
using PFN_FCICreate = decltype(&::FCICreate);
using PFN_FCIDestroy = decltype(&::FCIDestroy);
using PFN_FCIFlushCabinet = decltype(&::FCIFlushCabinet);

PFN_MsiCloseHandle pfn_MsiCloseHandle = nullptr;
PFN_MsiCreateRecord pfn_MsiCreateRecord = nullptr;
PFN_MsiDatabaseCommit pfn_MsiDatabaseCommit = nullptr;
PFN_MsiDatabaseIsTablePersistentW pfn_MsiDatabaseIsTablePersistentW = nullptr;
PFN_MsiDatabaseOpenViewW pfn_MsiDatabaseOpenViewW = nullptr;
PFN_MsiEnumRelatedProductsW pfn_MsiEnumRelatedProductsW = nullptr;
PFN_MsiGetProductInfoW pfn_MsiGetProductInfoW = nullptr;
PFN_MsiGetSummaryInformationW pfn_MsiGetSummaryInformationW = nullptr;
PFN_MsiOpenDatabaseW pfn_MsiOpenDatabaseW = nullptr;
PFN_MsiRecordGetInteger pfn_MsiRecordGetInteger = nullptr;
PFN_MsiRecordGetStringW pfn_MsiRecordGetStringW = nullptr;
PFN_MsiRecordSetInteger pfn_MsiRecordSetInteger = nullptr;
PFN_MsiRecordSetStreamW pfn_MsiRecordSetStreamW = nullptr;
PFN_MsiRecordSetStringW pfn_MsiRecordSetStringW = nullptr;
PFN_MsiSummaryInfoPersist pfn_MsiSummaryInfoPersist = nullptr;
PFN_MsiSummaryInfoSetPropertyW pfn_MsiSummaryInfoSetPropertyW = nullptr;
PFN_MsiViewExecute pfn_MsiViewExecute = nullptr;
PFN_MsiViewFetch pfn_MsiViewFetch = nullptr;
PFN_MsiViewModify pfn_MsiViewModify = nullptr;
PFN_FCIAddFile pfn_FCIAddFile = nullptr;
PFN_FCICreate pfn_FCICreate = nullptr;
PFN_FCIDestroy pfn_FCIDestroy = nullptr;
PFN_FCIFlushCabinet pfn_FCIFlushCabinet = nullptr;

class ScopedMsiHandle
{
public:
    ScopedMsiHandle() = default;
    explicit ScopedMsiHandle(MSIHANDLE handle)
        : handle_(handle)
    {
        // Nothing
    }

    ~ScopedMsiHandle()
    {
        reset();
    }

    MSIHANDLE* receive()
    {
        reset();
        return &handle_;
    }

    operator MSIHANDLE() const
    {
        return handle_;
    }

private:
    void reset()
    {
        if (handle_)
            pfn_MsiCloseHandle(handle_);
        handle_ = 0;
    }

    MSIHANDLE handle_ = 0;

    Q_DISABLE_COPY_MOVE(ScopedMsiHandle)
};

struct CabinetContext
{
    QString temp_dir;
    int temp_file_count = 0;
};

//--------------------------------------------------------------------------------------------------
template <typename Function>
bool resolve(HMODULE module, const char* name, Function* function)
{
    *function = reinterpret_cast<Function>(GetProcAddress(module, name));
    if (!*function)
    {
        LOG(ERROR) << "Unable to find" << name;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool loadFunctions()
{
    static const bool result = []()
    {
        HMODULE msi = LoadLibraryExW(L"msi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        HMODULE cabinet = LoadLibraryExW(L"cabinet.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!msi || !cabinet)
        {
            PLOG(ERROR) << "LoadLibraryExW failed";
            return false;
        }

        bool ok = true;
        ok &= resolve(msi, "MsiCloseHandle", &pfn_MsiCloseHandle);
        ok &= resolve(msi, "MsiCreateRecord", &pfn_MsiCreateRecord);
        ok &= resolve(msi, "MsiDatabaseCommit", &pfn_MsiDatabaseCommit);
        ok &= resolve(msi, "MsiDatabaseIsTablePersistentW", &pfn_MsiDatabaseIsTablePersistentW);
        ok &= resolve(msi, "MsiDatabaseOpenViewW", &pfn_MsiDatabaseOpenViewW);
        ok &= resolve(msi, "MsiEnumRelatedProductsW", &pfn_MsiEnumRelatedProductsW);
        ok &= resolve(msi, "MsiGetProductInfoW", &pfn_MsiGetProductInfoW);
        ok &= resolve(msi, "MsiGetSummaryInformationW", &pfn_MsiGetSummaryInformationW);
        ok &= resolve(msi, "MsiOpenDatabaseW", &pfn_MsiOpenDatabaseW);
        ok &= resolve(msi, "MsiRecordGetInteger", &pfn_MsiRecordGetInteger);
        ok &= resolve(msi, "MsiRecordGetStringW", &pfn_MsiRecordGetStringW);
        ok &= resolve(msi, "MsiRecordSetInteger", &pfn_MsiRecordSetInteger);
        ok &= resolve(msi, "MsiRecordSetStreamW", &pfn_MsiRecordSetStreamW);
        ok &= resolve(msi, "MsiRecordSetStringW", &pfn_MsiRecordSetStringW);
        ok &= resolve(msi, "MsiSummaryInfoPersist", &pfn_MsiSummaryInfoPersist);
        ok &= resolve(msi, "MsiSummaryInfoSetPropertyW", &pfn_MsiSummaryInfoSetPropertyW);
        ok &= resolve(msi, "MsiViewExecute", &pfn_MsiViewExecute);
        ok &= resolve(msi, "MsiViewFetch", &pfn_MsiViewFetch);
        ok &= resolve(msi, "MsiViewModify", &pfn_MsiViewModify);
        ok &= resolve(cabinet, "FCIAddFile", &pfn_FCIAddFile);
        ok &= resolve(cabinet, "FCICreate", &pfn_FCICreate);
        ok &= resolve(cabinet, "FCIDestroy", &pfn_FCIDestroy);
        ok &= resolve(cabinet, "FCIFlushCabinet", &pfn_FCIFlushCabinet);
        return ok;
    }();

    return result;
}

//--------------------------------------------------------------------------------------------------
void* DIAMONDAPI cabinetAlloc(ULONG size)
{
    return malloc(size);
}

//--------------------------------------------------------------------------------------------------
void DIAMONDAPI cabinetFree(void* memory)
{
    free(memory);
}

//--------------------------------------------------------------------------------------------------
INT_PTR DIAMONDAPI cabinetOpen(LPSTR path, int flags, int mode, int* error, void* /* context */)
{
    int file = -1;
    *error = _wsopen_s(&file, qUtf16Printable(QString::fromUtf8(path)), flags | _O_BINARY, _SH_DENYNO, mode);
    return file;
}

//--------------------------------------------------------------------------------------------------
UINT DIAMONDAPI cabinetRead(INT_PTR file, void* buffer, UINT size, int* error, void* /* context */)
{
    const UINT result = static_cast<UINT>(_read(static_cast<int>(file), buffer, size));
    if (result != size)
        *error = errno;
    return result;
}

//--------------------------------------------------------------------------------------------------
UINT DIAMONDAPI cabinetWrite(INT_PTR file, void* buffer, UINT size, int* error, void* /* context */)
{
    const UINT result = static_cast<UINT>(_write(static_cast<int>(file), buffer, size));
    if (result != size)
        *error = errno;
    return result;
}

//--------------------------------------------------------------------------------------------------
int DIAMONDAPI cabinetClose(INT_PTR file, int* error, void* /* context */)
{
    const int result = _close(static_cast<int>(file));
    if (result != 0)
        *error = errno;
    return result;
}

//--------------------------------------------------------------------------------------------------
long DIAMONDAPI cabinetSeek(INT_PTR file, long distance, int origin, int* error, void* /* context */)
{
    const long result = _lseek(static_cast<int>(file), distance, origin);
    if (result == -1)
        *error = errno;
    return result;
}

//--------------------------------------------------------------------------------------------------
int DIAMONDAPI cabinetDelete(LPSTR path, int* error, void* /* context */)
{
    const int result = _wremove(qUtf16Printable(QString::fromUtf8(path)));
    if (result != 0)
        *error = errno;
    return result;
}

//--------------------------------------------------------------------------------------------------
BOOL DIAMONDAPI cabinetTempFile(char* buffer, int size, void* context)
{
    CabinetContext* cabinet = static_cast<CabinetContext*>(context);

    const QByteArray path = QDir::toNativeSeparators(
        cabinet->temp_dir + QString("/temp%1").arg(++cabinet->temp_file_count)).toUtf8();
    if (path.size() >= size)
        return FALSE;

    memcpy(buffer, path.constData(), path.size() + 1);
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
INT_PTR DIAMONDAPI cabinetOpenInfo(LPSTR path, USHORT* date, USHORT* time, USHORT* attributes, int* error,
    void* context)
{
    FILETIME system_time;
    FILETIME local_time;
    GetSystemTimeAsFileTime(&system_time);
    FileTimeToLocalFileTime(&system_time, &local_time);
    FileTimeToDosDateTime(&local_time, date, time);

    *attributes = _A_NORMAL;
    return cabinetOpen(path, _O_RDONLY, 0, error, context);
}

//--------------------------------------------------------------------------------------------------
int DIAMONDAPI cabinetFilePlaced(PCCAB /* ccab */, LPSTR /* path */, long /* size */, BOOL /* continuation */,
    void* /* context */)
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
BOOL DIAMONDAPI cabinetNext(PCCAB /* ccab */, ULONG /* previous_size */, void* /* context */)
{
    return FALSE;
}

//--------------------------------------------------------------------------------------------------
long DIAMONDAPI cabinetStatus(UINT /* type */, ULONG /* size1 */, ULONG /* size2 */, void* /* context */)
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
bool createCabinet(const QString& source_path, const QString& cabinet_path)
{
    const QFileInfo cabinet_info(cabinet_path);

    CabinetContext context;
    context.temp_dir = cabinet_info.absolutePath();

    const QByteArray cabinet_dir = QDir::toNativeSeparators(context.temp_dir + '/').toUtf8();
    const QByteArray cabinet_name = cabinet_info.fileName().toUtf8();

    CCAB ccab;
    memset(&ccab, 0, sizeof(ccab));

    if (cabinet_dir.size() >= static_cast<int>(sizeof(ccab.szCabPath)) ||
        cabinet_name.size() >= static_cast<int>(sizeof(ccab.szCab)))
    {
        LOG(ERROR) << "Too long cabinet path:" << cabinet_path;
        return false;
    }

    ccab.cb = 0x7FFFFFFF;
    ccab.cbFolderThresh = 0x7FFFFFFF;
    memcpy(ccab.szCabPath, cabinet_dir.constData(), cabinet_dir.size() + 1);
    memcpy(ccab.szCab, cabinet_name.constData(), cabinet_name.size() + 1);

    ERF erf;
    memset(&erf, 0, sizeof(erf));

    HFCI fci = pfn_FCICreate(&erf, cabinetFilePlaced, cabinetAlloc, cabinetFree, cabinetOpen, cabinetRead,
                         cabinetWrite, cabinetClose, cabinetSeek, cabinetDelete, cabinetTempFile, &ccab,
                         &context);
    if (!fci)
    {
        LOG(ERROR) << "FCICreate failed:" << erf.erfOper;
        return false;
    }

    QByteArray source = QDir::toNativeSeparators(source_path).toUtf8();
    QByteArray name(kSettingsFile);

    const bool result =
        pfn_FCIAddFile(fci, source.data(), name.data(), FALSE, cabinetNext, cabinetStatus, cabinetOpenInfo,
                   tcompTYPE_MSZIP) &&
        pfn_FCIFlushCabinet(fci, FALSE, cabinetNext, cabinetStatus);
    if (!result)
        LOG(ERROR) << "Unable to create cabinet:" << erf.erfOper;

    pfn_FCIDestroy(fci);
    return result;
}

//--------------------------------------------------------------------------------------------------
QString installedPackage()
{
    wchar_t product_code[39];
    UINT error = pfn_MsiEnumRelatedProductsW(kUpgradeCode, 0, 0, product_code);
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiEnumRelatedProductsW failed:" << SystemError::toString(error);
        return QString();
    }

    wchar_t path[MAX_PATH];
    DWORD size = static_cast<DWORD>(std::size(path));
    error = pfn_MsiGetProductInfoW(product_code, INSTALLPROPERTY_LOCALPACKAGE, path, &size);
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiGetProductInfoW failed:" << SystemError::toString(error);
        return QString();
    }

    return QString::fromWCharArray(path, static_cast<int>(size));
}

//--------------------------------------------------------------------------------------------------
bool executeQuery(MSIHANDLE database, const QString& query, MSIHANDLE params = 0)
{
    ScopedMsiHandle view;
    UINT error = pfn_MsiDatabaseOpenViewW(database, qUtf16Printable(query), view.receive());
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiDatabaseOpenViewW failed:" << SystemError::toString(error) << query;
        return false;
    }

    error = pfn_MsiViewExecute(view, params);
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiViewExecute failed:" << SystemError::toString(error) << query;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
MSIHANDLE fetchRecord(MSIHANDLE database, const QString& query)
{
    ScopedMsiHandle view;
    if (pfn_MsiDatabaseOpenViewW(database, qUtf16Printable(query), view.receive()) != ERROR_SUCCESS ||
        pfn_MsiViewExecute(view, 0) != ERROR_SUCCESS)
    {
        return 0;
    }

    MSIHANDLE record = 0;
    if (pfn_MsiViewFetch(view, &record) != ERROR_SUCCESS)
        return 0;

    return record;
}

//--------------------------------------------------------------------------------------------------
QString recordString(MSIHANDLE record, UINT field)
{
    DWORD size = 0;
    if (pfn_MsiRecordGetStringW(record, field, nullptr, &size) != ERROR_SUCCESS)
        return QString();

    std::wstring buffer(size, L'\0');
    ++size;

    if (pfn_MsiRecordGetStringW(record, field, buffer.data(), &size) != ERROR_SUCCESS)
        return QString();

    return QString::fromStdWString(buffer);
}

//--------------------------------------------------------------------------------------------------
void setRecordString(MSIHANDLE record, UINT field, const QString& value)
{
    pfn_MsiRecordSetStringW(record, field, qUtf16Printable(value));
}

//--------------------------------------------------------------------------------------------------
bool lockSettingsFile(MSIHANDLE database)
{
    if (pfn_MsiDatabaseIsTablePersistentW(database, L"MsiLockPermissionsEx") == MSICONDITION_NONE &&
        !executeQuery(database,
            "CREATE TABLE `MsiLockPermissionsEx` (`MsiLockPermissionsEx` CHAR(72) NOT NULL, "
            "`LockObject` CHAR(72) NOT NULL, `Table` CHAR(32) NOT NULL, `SDDLText` LONGCHAR NOT NULL, "
            "`Condition` CHAR(255) PRIMARY KEY `MsiLockPermissionsEx`)"))
    {
        return false;
    }

    ScopedMsiHandle row(pfn_MsiCreateRecord(3));
    setRecordString(row, 1, kSettingsFile);
    setRecordString(row, 2, kSettingsFile);
    setRecordString(row, 3, kSettingsFileSddl);

    return executeQuery(database,
        "INSERT INTO `MsiLockPermissionsEx` (`MsiLockPermissionsEx`, `LockObject`, `Table`, `SDDLText`) "
        "VALUES (?, ?, 'File', ?)", row);
}

//--------------------------------------------------------------------------------------------------
bool addSettingsFile(MSIHANDLE database, const QString& executable_component)
{
    ScopedMsiHandle component(fetchRecord(database,
        QString("SELECT `Directory_`, `Attributes` FROM `Component` WHERE `Component`='%1'").arg(executable_component)));
    ScopedMsiHandle feature(fetchRecord(database,
        QString("SELECT `Feature_` FROM `FeatureComponents` WHERE `Component_`='%1'").arg(executable_component)));
    if (!component || !feature)
    {
        LOG(ERROR) << "No component or feature of the executable";
        return false;
    }

    ScopedMsiHandle view;
    if (pfn_MsiDatabaseOpenViewW(database, L"SELECT `DiskId`, `LastSequence` FROM `Media`",
                                 view.receive()) != ERROR_SUCCESS ||
        pfn_MsiViewExecute(view, 0) != ERROR_SUCCESS)
    {
        LOG(ERROR) << "Unable to read the media of the package";
        return false;
    }

    int disk_id = 0;
    int sequence = 0;

    ScopedMsiHandle media;
    while (pfn_MsiViewFetch(view, media.receive()) == ERROR_SUCCESS)
    {
        disk_id = qMax(disk_id, pfn_MsiRecordGetInteger(media, 1));
        sequence = qMax(sequence, pfn_MsiRecordGetInteger(media, 2));
    }

    ++disk_id;
    ++sequence;

    ScopedMsiHandle component_row(pfn_MsiCreateRecord(5));
    setRecordString(component_row, 1, kSettingsComponent);
    setRecordString(component_row, 2, kSettingsComponentId);
    setRecordString(component_row, 3, recordString(component, 1));
    pfn_MsiRecordSetInteger(component_row, 4, pfn_MsiRecordGetInteger(component, 2));
    setRecordString(component_row, 5, kSettingsFile);

    ScopedMsiHandle feature_row(pfn_MsiCreateRecord(2));
    setRecordString(feature_row, 1, recordString(feature, 1));
    setRecordString(feature_row, 2, kSettingsComponent);

    ScopedMsiHandle file_row(pfn_MsiCreateRecord(5));
    setRecordString(file_row, 1, kSettingsFile);
    setRecordString(file_row, 2, kSettingsComponent);
    setRecordString(file_row, 3, kSettingsFileName);
    pfn_MsiRecordSetInteger(file_row, 4, msidbFileAttributesVital);
    pfn_MsiRecordSetInteger(file_row, 5, sequence);

    ScopedMsiHandle media_row(pfn_MsiCreateRecord(3));
    pfn_MsiRecordSetInteger(media_row, 1, disk_id);
    pfn_MsiRecordSetInteger(media_row, 2, sequence);
    setRecordString(media_row, 3, QString("#") + kSettingsCabinet);

    ScopedMsiHandle action_row(pfn_MsiCreateRecord(1));
    setRecordString(action_row, 1, QString("--import=\"[#%1]\" --silent").arg(kSettingsFile));

    return executeQuery(database, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, "
                                  "`KeyPath`) VALUES (?, ?, ?, ?, ?)", component_row) &&
           executeQuery(database, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES (?, ?)",
                        feature_row) &&
           executeQuery(database, "INSERT INTO `File` (`File`, `Component_`, `FileName`, `FileSize`, `Attributes`, "
                                  "`Sequence`) VALUES (?, ?, ?, 0, ?, ?)", file_row) &&
           executeQuery(database, "INSERT INTO `Media` (`DiskId`, `LastSequence`, `Cabinet`) VALUES (?, ?, ?)",
                        media_row) &&
           lockSettingsFile(database) &&
           executeQuery(database, QString("UPDATE `CustomAction` SET `Target`=? WHERE `Action`='%1'").arg(kImportAction),
                        action_row);
}

//--------------------------------------------------------------------------------------------------
bool replaceCabinet(MSIHANDLE database, const QString& cabinet_path)
{
    ScopedMsiHandle view;
    UINT error = pfn_MsiDatabaseOpenViewW(database, L"SELECT `Name`, `Data` FROM `_Streams`", view.receive());
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiDatabaseOpenViewW failed:" << SystemError::toString(error);
        return false;
    }

    error = pfn_MsiViewExecute(view, 0);
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiViewExecute failed:" << SystemError::toString(error);
        return false;
    }

    ScopedMsiHandle record(pfn_MsiCreateRecord(2));
    setRecordString(record, 1, kSettingsCabinet);

    error = pfn_MsiRecordSetStreamW(record, 2, qUtf16Printable(cabinet_path));
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiRecordSetStreamW failed:" << SystemError::toString(error);
        return false;
    }

    error = pfn_MsiViewModify(view, MSIMODIFY_ASSIGN, record);
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiViewModify failed:" << SystemError::toString(error);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool updatePackageCode(MSIHANDLE database)
{
    ScopedMsiHandle summary;
    UINT error = pfn_MsiGetSummaryInformationW(database, nullptr, 1, summary.receive());
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiGetSummaryInformationW failed:" << SystemError::toString(error);
        return false;
    }

    const QString package_code = QUuid::createUuid().toString(QUuid::WithBraces).toUpper();

    error = pfn_MsiSummaryInfoSetPropertyW(summary, PID_REVNUMBER, VT_LPSTR, 0, nullptr, qUtf16Printable(package_code));
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiSummaryInfoSetPropertyW failed:" << SystemError::toString(error);
        return false;
    }

    error = pfn_MsiSummaryInfoPersist(summary);
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiSummaryInfoPersist failed:" << SystemError::toString(error);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
MsiPackage::Result embedSettings(const QString& package_path, const QString& settings_path,
    const QString& cabinet_path)
{
    ScopedMsiHandle database;
    UINT error = pfn_MsiOpenDatabaseW(qUtf16Printable(package_path), MSIDBOPEN_TRANSACT, database.receive());
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiOpenDatabaseW failed:" << SystemError::toString(error);
        return MsiPackage::Result::FAILED;
    }

    ScopedMsiHandle executable(fetchRecord(database,
        QString("SELECT `Component_` FROM `File` WHERE `File`='%1'").arg(kExecutableFile)));
    ScopedMsiHandle action(fetchRecord(database,
        QString("SELECT `Action` FROM `CustomAction` WHERE `Action`='%1'").arg(kImportAction)));
    if (!executable || !action)
    {
        LOG(ERROR) << "The installed package has no executable or import action";
        return MsiPackage::Result::UNSUPPORTED_PACKAGE;
    }

    // A package exported before already carries the settings file.
    ScopedMsiHandle settings(fetchRecord(database,
        QString("SELECT `File` FROM `File` WHERE `File`='%1'").arg(kSettingsFile)));
    if (!settings && !addSettingsFile(database, recordString(executable, 1)))
        return MsiPackage::Result::FAILED;

    ScopedMsiHandle size_row(pfn_MsiCreateRecord(1));
    pfn_MsiRecordSetInteger(size_row, 1, static_cast<int>(QFileInfo(settings_path).size()));

    // A package with other contents must not share the package code of the installed one.
    if (!executeQuery(database,
            QString("UPDATE `File` SET `FileSize`=? WHERE `File`='%1'").arg(kSettingsFile), size_row) ||
        !replaceCabinet(database, cabinet_path) || !updatePackageCode(database))
    {
        return MsiPackage::Result::FAILED;
    }

    error = pfn_MsiDatabaseCommit(database);
    if (error != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiDatabaseCommit failed:" << SystemError::toString(error);
        return MsiPackage::Result::FAILED;
    }

    return MsiPackage::Result::SUCCESS;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
MsiPackage::Result MsiPackage::exportWithSettings(const QString& file_path)
{
    if (!loadFunctions())
        return Result::FAILED;

    const QString package_path = installedPackage();
    if (package_path.isEmpty())
        return Result::NO_PACKAGE;

    LOG(INFO) << "Installed package:" << package_path;

    QTemporaryDir temp_dir;
    if (!temp_dir.isValid())
    {
        LOG(ERROR) << "Unable to create temporary directory:" << temp_dir.errorString();
        return Result::FAILED;
    }

    const QString settings_path = temp_dir.filePath(kSettingsFile);
    const QString cabinet_path = temp_dir.filePath(kSettingsCabinet);

    if (!SettingsUtil::exportToFile(settings_path, true) || !createCabinet(settings_path, cabinet_path))
        return Result::FAILED;

    if (QFile::exists(file_path) && !QFile::remove(file_path))
    {
        LOG(ERROR) << "Unable to remove existing file:" << file_path;
        return Result::FAILED;
    }

    if (!QFile::copy(package_path, file_path))
    {
        LOG(ERROR) << "Unable to copy package to:" << file_path;
        return Result::FAILED;
    }

    const Result result = embedSettings(file_path, settings_path, cabinet_path);
    if (result != Result::SUCCESS)
        QFile::remove(file_path);

    return result;
}
