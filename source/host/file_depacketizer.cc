//
// PROJECT:         Aspia
// FILE:            host/file_depacketizer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_depacketizer.h"

#include <QDebug>

namespace aspia {

FileDepacketizer::FileDepacketizer(QPointer<QFile>& file)
{
    file_.swap(file);
}

// static
std::unique_ptr<FileDepacketizer> FileDepacketizer::create(
    const QString& file_path, bool overwrite)
{
    QFile::OpenMode mode = QFile::WriteOnly;

    if (overwrite)
        mode |= QFile::Truncate;

    QPointer<QFile> file = new QFile(file_path);

    if (!file->open(mode))
        return nullptr;

    return std::unique_ptr<FileDepacketizer>(new FileDepacketizer(file));
}

bool FileDepacketizer::writeNextPacket(const proto::file_transfer::Packet& packet)
{
    Q_ASSERT(!file_.isNull() && file_->isOpen());

    // The first packet must have the full file size.
    if (packet.flags() & proto::file_transfer::Packet::FLAG_FIRST_PACKET)
    {
        file_size_ = packet.file_size();
        left_size_ = file_size_;
    }

    const size_t packet_size = packet.data().size();

    if (!file_->seek(file_size_ - left_size_))
    {
        qDebug("seek failed");
        return false;
    }

    if (file_->write(packet.data().data(), packet_size) != packet_size)
    {
        qDebug("Unable to write file");
        return false;
    }

    left_size_ -= packet_size;

    if (packet.flags() & proto::file_transfer::Packet::FLAG_LAST_PACKET)
    {
        file_size_ = 0;
        file_->close();
    }

    return true;
}

} // namespace aspia
