//
// PROJECT:         Aspia
// FILE:            ui/mru.cc
// LICENSE:         GNU Lesser General Public License 2.1
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

std::optional<std::experimental::filesystem::path> GetMRUFilePath()
{
    auto path = BasePaths::GetAppDataDirectory();
    if (!path.has_value())
        return {};

    path->append(L"Aspia");
    path->append(L"mru.dat");

    return path;
}

std::optional<std::string> ReadMRUFile()
{
    auto file_path = GetMRUFilePath();
    if (!file_path.has_value())
        return {};

    std::ifstream file_stream;

    file_stream.open(file_path.value(), std::ifstream::binary);
    if (!file_stream.is_open())
    {
        DLOG(LS_WARNING) << "File not found: " << *file_path;
        return {};
    }

    file_stream.seekg(0, file_stream.end);
    std::streamoff size = static_cast<size_t>(file_stream.tellg());
    file_stream.seekg(0);

    if (!size)
        return {};

    if (size > kMaxMRUFileSize)
    {
        LOG(LS_ERROR) << "MRU is too large (>1MB)";
        return {};
    }

    std::string mru;
    mru.resize(static_cast<size_t>(size));

    file_stream.read(mru.data(), size);

    if (file_stream.fail())
    {
        LOG(LS_ERROR) << "Unable to read MRU";
        return {};
    }

    file_stream.close();
    return mru;
}

bool WriteMRUFile(const std::string& mru)
{
    auto file_path = GetMRUFilePath();
    if (!file_path.has_value())
        return false;

    std::ofstream file_stream;

    file_stream.open(file_path.value(), std::ofstream::binary);
    if (!file_stream.is_open())
    {
        LOG(LS_ERROR) << "Unable to create (or open) file: " << *file_path;
        return false;
    }

    file_stream.write(mru.c_str(), mru.size());
    if (file_stream.fail())
    {
        LOG(LS_ERROR) << "Unable to write user list";
        return false;
    }

    file_stream.close();
    return true;
}

} // namespace

MRU::MRU()
{
    std::optional<std::string> string = ReadMRUFile();
    if (!string.has_value())
        return;

    if (!mru_.ParseFromString(string.value()))
    {
        LOG(LS_ERROR) << "Unable to parse MRU file";
        return;
    }

    if (mru_.computer_size() > kMaxMRUItemCount)
    {
        mru_.mutable_computer()->DeleteSubrange(
            0, mru_.computer_size() - kMaxMRUFileSize);
    }
}

MRU::~MRU()
{
    WriteMRUFile(mru_.SerializeAsString());
}

// static
proto::Computer MRU::GetDefaultConfig()
{
    proto::Computer computer;

    computer.set_port(kDefaultHostTcpPort);
    computer.set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);

    proto::desktop::Config* desktop_manage_session = computer.mutable_desktop_manage_session();

    desktop_manage_session->set_flags(proto::desktop::Config::ENABLE_CLIPBOARD |
                                      proto::desktop::Config::ENABLE_CURSOR_SHAPE);
    desktop_manage_session->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_manage_session->set_update_interval(30);
    desktop_manage_session->set_compress_ratio(6);

    ConvertToVideoPixelFormat(PixelFormat::RGB565(),
                              desktop_manage_session->mutable_pixel_format());

    proto::desktop::Config* desktop_view_session = computer.mutable_desktop_view_session();

    desktop_view_session->set_flags(0);
    desktop_view_session->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_view_session->set_update_interval(30);
    desktop_view_session->set_compress_ratio(6);

    ConvertToVideoPixelFormat(PixelFormat::RGB565(),
                              desktop_view_session->mutable_pixel_format());

    return computer;
}

void MRU::AddItem(const proto::Computer& computer)
{
    if (computer.address().empty())
        return;

    for (int index = 0; index < mru_.computer_size(); ++index)
    {
        if (computer.address() == mru_.computer(index).address())
        {
            // Item already in the list. Remove from current position.
            mru_.mutable_computer()->DeleteSubrange(index, 1);
            break;
        }
    }

    // Add item to end.
    mru_.add_computer()->CopyFrom(computer);

    if (mru_.computer_size() > kMaxMRUItemCount)
    {
        // Remove first (oldest) item.
        mru_.mutable_computer()->DeleteSubrange(0, 1);
    }
}

int MRU::GetItemCount() const
{
    return mru_.computer_size();
}

const proto::Computer& MRU::GetItem(int item_index) const
{
    DCHECK(mru_.computer_size() != 0 && item_index < mru_.computer_size());
    return mru_.computer(item_index);
}

const proto::Computer& MRU::SetLastItem(int item_index)
{
    DCHECK(mru_.computer_size() != 0 && item_index < mru_.computer_size());

    // Save item.
    proto::Computer computer = mru_.computer(item_index);

    // Delete item from current position.
    mru_.mutable_computer()->DeleteSubrange(item_index, 1);

    // Add item to end.
    mru_.add_computer()->CopyFrom(computer);

    // Get end item.
    return GetItem(GetItemCount() - 1);
}

} // namespace aspia
