//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/file_version_info.h"

#include "base/logging.h"
#include "base/win/resource_util.h"

namespace base {

namespace {

struct LanguageAndCodePage
{
    WORD language;
    WORD code_page;
};

//--------------------------------------------------------------------------------------------------
// Returns the \\VarFileInfo\\Translation value extracted from the VS_VERSION_INFO resource in
// |data|.
LanguageAndCodePage* GetTranslate(const void* data)
{
    LanguageAndCodePage* translate = nullptr;
    UINT length;

    if (VerQueryValueW(data, L"\\VarFileInfo\\Translation",
                       reinterpret_cast<void**>(&translate), &length))
    {
        return translate;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
VS_FIXEDFILEINFO* GetVsFixedFileInfo(const void* data)
{
    VS_FIXEDFILEINFO* fixed_file_info = nullptr;
    UINT length;

    if (VerQueryValueW(data, L"\\", reinterpret_cast<void**>(&fixed_file_info), &length))
        return fixed_file_info;

    return nullptr;
}

} // namespace

//--------------------------------------------------------------------------------------------------
FileVersionInfo::FileVersionInfo(QVector<quint8>&& data, WORD language, WORD code_page)
    : owned_data_(std::move(data)),
      data_(owned_data_.data()),
      language_(language),
      code_page_(code_page),
      fixed_file_info_(GetVsFixedFileInfo(data_))
{
    DCHECK(!owned_data_.empty());
}

//--------------------------------------------------------------------------------------------------
FileVersionInfo::FileVersionInfo(void* data, WORD language, WORD code_page)
    : data_(data),
      language_(language),
      code_page_(code_page),
      fixed_file_info_(GetVsFixedFileInfo(data))
{
    DCHECK(data_);
}

//--------------------------------------------------------------------------------------------------
FileVersionInfo::~FileVersionInfo() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FileVersionInfo> FileVersionInfo::createFileVersionInfoForModule(
    HMODULE module)
{
    void* data;
    size_t version_info_length;

    const bool has_version_resource = base::resourceFromModule(
        module, VS_VERSION_INFO, RT_VERSION, &data, &version_info_length);
    if (!has_version_resource)
        return nullptr;

    const LanguageAndCodePage* translate = GetTranslate(data);
    if (!translate)
        return nullptr;

    return std::unique_ptr<FileVersionInfo>(
        new FileVersionInfo(data, translate->language, translate->code_page));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FileVersionInfo> FileVersionInfo::createFileVersionInfo(const QString& file_path)
{
    DWORD dummy;
    const wchar_t* path = qUtf16Printable(file_path);

    const DWORD length = GetFileVersionInfoSizeW(path, &dummy);
    if (length == 0)
        return nullptr;

    QVector<quint8> data(length, 0);

    if (!GetFileVersionInfoW(path, dummy, length, data.data()))
    {
        PLOG(LS_ERROR) << "GetFileVersionInfoW failed";
        return nullptr;
    }

    const LanguageAndCodePage* translate = GetTranslate(data.data());
    if (!translate)
        return nullptr;

    return std::unique_ptr<FileVersionInfo>(
        new FileVersionInfo(std::move(data), translate->language, translate->code_page));
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::companyName()
{
    return stringValue(L"CompanyName");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::companyShortName()
{
    return stringValue(L"CompanyShortName");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::internalName()
{
    return stringValue(L"InternalName");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::productName()
{
    return stringValue(L"ProductName");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::productShortName()
{
    return stringValue(L"ProductShortName");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::comments()
{
    return stringValue(L"Comments");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::legalCopyright()
{
    return stringValue(L"LegalCopyright");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::productVersion()
{
    return stringValue(L"ProductVersion");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::fileDescription()
{
    return stringValue(L"FileDescription");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::legalTrademarks()
{
    return stringValue(L"LegalTrademarks");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::privateBuild()
{
    return stringValue(L"PrivateBuild");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::fileVersion()
{
    return stringValue(L"FileVersion");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::originalFilename()
{
    return stringValue(L"OriginalFilename");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::specialBuild()
{
    return stringValue(L"SpecialBuild");
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::lastChange()
{
    return stringValue(L"LastChange");
}

//--------------------------------------------------------------------------------------------------
bool FileVersionInfo::isOfficialBuild()
{
    return (stringValue(L"Official Build").compare(L"1") == 0);
}

//--------------------------------------------------------------------------------------------------
bool FileVersionInfo::value(const wchar_t* name, QString* value_str)
{
    WORD lang_codepage[8];
    size_t i = 0;

    // Use the language and codepage from the DLL.
    lang_codepage[i++] = language_;
    lang_codepage[i++] = code_page_;
    // Use the default language and codepage from the DLL.
    lang_codepage[i++] = ::GetUserDefaultLangID();
    lang_codepage[i++] = code_page_;
    // Use the language from the DLL and Latin codepage (most common).
    lang_codepage[i++] = language_;
    lang_codepage[i++] = 1252;
    // Use the default language and Latin codepage (most common).
    lang_codepage[i++] = ::GetUserDefaultLangID();
    lang_codepage[i++] = 1252;

    i = 0;

    while (i < std::size(lang_codepage))
    {
        wchar_t sub_block[MAX_PATH];
        WORD language = lang_codepage[i++];
        WORD code_page = lang_codepage[i++];

        _snwprintf_s(sub_block, MAX_PATH, MAX_PATH,
                     L"\\StringFileInfo\\%04x%04x\\%ls", language, code_page, name);

        LPVOID value = nullptr;
        quint32 size;

        BOOL r = VerQueryValueW(data_, sub_block, &value, &size);
        if (r && value)
        {
            *value_str = QString::fromWCharArray(static_cast<wchar_t*>(value));
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
QString FileVersionInfo::stringValue(const wchar_t* name)
{
    QString str;

    if (value(name, &str))
        return str;
    else
        return "";
}

} // namespace base
