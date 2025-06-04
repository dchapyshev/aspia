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

#include "tools/sha256/sha256.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <Windows.h>

// Architecture detection.
#if defined(_M_X64) || defined(__x86_64__)

#define ARCH_CPU_STRING        "x86_64"
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_STRING        "x86"
#elif defined(__ARMEL__)
#define ARCH_CPU_STRING        "arm"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_CPU_STRING        "arm64"
#else
#error Unknown architecture
#endif

//--------------------------------------------------------------------------------------------------
std::filesystem::path currentDir()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer))))
        return std::filesystem::path();

    std::filesystem::path path(buffer);
    return path.parent_path();
}

//--------------------------------------------------------------------------------------------------
size_t calcFileSize(std::string_view file_name)
{
    if (file_name.empty())
        return 0;

    std::filesystem::path file_path = currentDir();
    if (file_path.empty())
        return 0;

    file_path.append(file_name);

    std::error_code error_code;
    size_t size = std::filesystem::file_size(file_path, error_code);
    if (error_code)
        return 0;

    return size;
}

//--------------------------------------------------------------------------------------------------
std::string calcFileHash(std::string_view file_name)
{
    if (file_name.empty())
        return std::string();

    std::filesystem::path file_path = currentDir();
    if (file_path.empty())
        return std::string();

    file_path.append(file_name);

    std::ifstream stream;
    stream.open(file_path, std::ifstream::binary);
    if (!stream.is_open())
        return std::string();

    sha256 sha;
    sha256_init(&sha);

    while (!stream.eof())
    {
        std::array<char, 1024> buffer;
        stream.read(buffer.data(), buffer.size());
        sha256_append(&sha, buffer.data(), stream.gcount());
    }

    char hex[SHA256_HEX_SIZE] = { 0 };
    sha256_finalize_hex(&sha, hex);
    return hex;
}

//--------------------------------------------------------------------------------------------------
std::string dash(size_t count)
{
    std::string result;

    for (size_t i = 0; i < count; ++i)
        result += '-';

    return result;
}

//--------------------------------------------------------------------------------------------------
int main(int /* argc */, const char* const* /* argv */)
{
    const size_t kFileNameWidth = 32;
    const size_t kSizeWidth = 10;
    const size_t kSHA256Width = 64;

    const char* kFilesTable[] =
    {
        "aspia_client.exe",
        "aspia_console.exe",
        "aspia_desktop_agent.exe",
        "aspia_file_transfer_agent.exe",
        "aspia_host.exe",
        "aspia_host_core.dll",
        "aspia_host_service.exe",
        "aspia_relay.exe",
        "aspia_router.exe"
    };

    const char* kInstallersTable[] =
    {
        "aspia-client",
        "aspia-console",
        "aspia-host",
        "aspia-relay",
        "aspia-router"
    };

    std::cout << '|' << std::setw(kFileNameWidth) << "File Name"
              << '|' << std::setw(kSizeWidth) << "Size"
              << '|' << std::setw(kSHA256Width) << "SHA256"
              << '|' << std::endl;
    std::cout << '|' << dash(kFileNameWidth)
              << '|' << dash(kSizeWidth)
              << '|' << dash(kSHA256Width)
              << '|' << std::endl;

    for (auto it = std::begin(kFilesTable); it != std::end(kFilesTable); ++it)
    {
        std::string_view file_name = *it;

        std::cout << '|' << std::setw(kFileNameWidth) << file_name
                  << '|' << std::setw(kSizeWidth) << calcFileSize(file_name)
                  << '|' << std::setw(kSHA256Width) << calcFileHash(file_name)
                  << '|' << std::endl;
    }

    for (auto it = std::begin(kInstallersTable); it != std::end(kInstallersTable); ++it)
    {
        std::string_view prefix = *it;

        std::ostringstream file_name_stream;
        file_name_stream << prefix
                         << '-' << ASPIA_VERSION_MAJOR
                         << '.' << ASPIA_VERSION_MINOR
                         << '.' << ASPIA_VERSION_PATCH
                         << '-' << ARCH_CPU_STRING
                         << ".msi";

        std::string file_name = file_name_stream.str();

        std::cout << '|' << std::setw(kFileNameWidth) << file_name
                  << '|' << std::setw(kSizeWidth) << calcFileSize(file_name)
                  << '|' << std::setw(kSHA256Width) << calcFileHash(file_name)
                  << '|' << std::endl;
    }

    return 0;
}
