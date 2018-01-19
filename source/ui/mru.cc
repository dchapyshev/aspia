//
// PROJECT:         Aspia
// FILE:            ui/mru.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/base_paths.h"
#include "base/logging.h"
#include "codec/video_helpers.h"
#include "ui/mru.h"

#include <fstream>

namespace aspia {

namespace {

constexpr size_t kMaxMRUFileSize = 1024 * 1024; // 1 MB
constexpr size_t kMaxMRUItemCount = 30;

bool GetMRUFilePath(std::experimental::filesystem::path& path)
{
    if (!GetBasePath(BasePathKey::DIR_APP_DATA, path))
        return false;

    path.append(L"Aspia");
    path.append(L"mru.dat");

    return true;
}

bool ReadMRUFile(std::string& mru)
{
    std::experimental::filesystem::path file_path;

    if (!GetMRUFilePath(file_path))
        return false;

    std::ifstream file_stream;

    file_stream.open(file_path, std::ifstream::binary);
    if (!file_stream.is_open())
    {
        DLOG(WARNING) << "File not found: " << file_path;
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
    std::experimental::filesystem::path file_path;

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
    std::string string;

    if (!ReadMRUFile(string))
        return;

    if (!mru_.ParseFromString(string))
    {
        LOG(ERROR) << "Unable to parse MRU file";
        return;
    }

    if (mru_.client_config_size() > kMaxMRUItemCount)
    {
        mru_.mutable_client_config()->DeleteSubrange(
            0, mru_.client_config_size() - kMaxMRUFileSize);
    }
}

MRU::~MRU()
{
    WriteMRUFile(mru_.SerializeAsString());
}

// static
proto::ClientConfig MRU::GetDefaultConfig()
{
    proto::ClientConfig config;

    config.set_port(kDefaultHostTcpPort);
    config.set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);

    proto::desktop::Config* desktop_session_config = config.mutable_desktop_session();

    desktop_session_config->set_flags(proto::desktop::Config::ENABLE_CLIPBOARD |
                                      proto::desktop::Config::ENABLE_CURSOR_SHAPE);
    desktop_session_config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_session_config->set_update_interval(30);
    desktop_session_config->set_compress_ratio(6);

    ConvertToVideoPixelFormat(PixelFormat::RGB565(),
                              desktop_session_config->mutable_pixel_format());

    return config;
}

void MRU::AddItem(const proto::ClientConfig& client_config)
{
    if (client_config.address().empty())
        return;

    for (int index = 0; index < mru_.client_config_size(); ++index)
    {
        if (client_config.address() == mru_.client_config(index).address())
        {
            // Item already in the list. Remove from current position.
            mru_.mutable_client_config()->DeleteSubrange(index, 1);
            break;
        }
    }

    // Add item to end.
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

const proto::ClientConfig& MRU::SetLastItem(int item_index)
{
    DCHECK(mru_.client_config_size() != 0 && item_index < mru_.client_config_size());

    // Save item.
    proto::ClientConfig client_config = mru_.client_config(item_index);

    // Delete item from current position.
    mru_.mutable_client_config()->DeleteSubrange(item_index, 1);

    // Add item to end.
    mru_.add_client_config()->CopyFrom(client_config);

    // Get end item.
    return GetItem(GetItemCount() - 1);
}

} // namespace aspia
