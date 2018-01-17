//
// PROJECT:         Aspia
// FILE:            ui/mru.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/base_paths.h"
#include "base/logging.h"
#include "ui/mru.h"

#include <fstream>

namespace aspia {

namespace {

constexpr size_t kMaxMRUFileSize = 1024 * 1024; // 1 MB
constexpr size_t kMaxMRUItemCount = 50;

bool GetMRUFilePath(FilePath& path)
{
    if (!GetBasePath(BasePathKey::DIR_APP_DATA, path))
        return false;

    path.append(L"Aspia");
    path.append(L"mru.dat");

    return true;
}

bool ReadMRUFile(std::string& mru)
{
    FilePath file_path;

    if (!GetMRUFilePath(file_path))
        return false;

    std::ifstream file_stream;

    file_stream.open(file_path, std::ifstream::binary);
    if (!file_stream.is_open())
    {
        LOG(ERROR) << "File not found: " << file_path;
        return false;
    }

    file_stream.seekg(0, file_stream.end);
    std::streamoff size = static_cast<size_t>(file_stream.tellg());
    file_stream.seekg(0);

    if (!size)
        return false;

    if (size > kMaxMRUFileSize)
    {
        LOG(ERROR) << "MRU is too large (>1MB)";
        return false;
    }

    mru.resize(static_cast<size_t>(size));

    file_stream.read(mru.data(), size);

    if (file_stream.fail())
    {
        LOG(ERROR) << "Unable to read MRU";
        return false;
    }

    file_stream.close();

    return true;
}

bool WriteMRUFile(const std::string& mru)
{
    FilePath file_path;

    if (!GetMRUFilePath(file_path))
        return false;

    std::ofstream file_stream;

    file_stream.open(file_path, std::ofstream::binary);
    if (!file_stream.is_open())
    {
        LOG(ERROR) << "Unable to create (or open) file: " << file_path;
        return false;
    }

    file_stream.write(mru.c_str(), mru.size());
    if (file_stream.fail())
    {
        LOG(ERROR) << "Unable to write user list";
        return false;
    }

    file_stream.close();

    return true;
}

} // namespace

MRU::MRU()
{
    std::string mru;

    if (!ReadMRUFile(mru))
        return;

    if (!mru_.ParseFromString(mru))
    {
        LOG(ERROR) << "Unable to parse MRU file";
        return;
    }

    if (mru_.client_config_size() > kMaxMRUItemCount)
    {
        mru_.mutable_client_config()
            ->DeleteSubrange(0, mru_.client_config_size() - kMaxMRUFileSize);
    }
}

MRU::~MRU()
{
    WriteMRUFile(mru_.SerializeAsString());
}

void MRU::AddItem(const proto::ClientConfig& client_config)
{
    for (int index = 0; index < mru_.client_config_size(); ++index)
    {
        if (client_config.address() == mru_.client_config(index).address())
        {
            // The host is already in the list. We update session parameters.
            mru_.mutable_client_config(index)->CopyFrom(client_config);
            return;
        }
    }

    mru_.add_client_config()->CopyFrom(client_config);

    if (mru_.client_config_size() > kMaxMRUItemCount)
    {
        // Remove first (oldest) item.
        mru_.mutable_client_config()->DeleteSubrange(0, 1);
    }
}

int MRU::GetItemCount() const
{
    return mru_.client_config_size();
}

const proto::ClientConfig& MRU::GetItem(int item_index) const
{
    DCHECK(mru_.client_config_size() != 0 && item_index < mru_.client_config_size());
    return mru_.client_config(item_index);
}

} // namespace aspia
