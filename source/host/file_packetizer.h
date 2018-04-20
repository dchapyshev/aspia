//
// PROJECT:         Aspia
// FILE:            host/file_packetizer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__FILE_PACKETIZER_H
#define _ASPIA_HOST__FILE_PACKETIZER_H

#include <QFile>
#include <QPointer>
#include <QString>
#include <memory>

#include "protocol/file_transfer_session.pb.h"

namespace aspia {

class FilePacketizer
{
public:
    ~FilePacketizer() = default;

    // Creates an instance of the class.
    // Parameter |file_path| contains the full path to the file.
    // If the specified file can not be opened for reading, then returns nullptr.
    static std::unique_ptr<FilePacketizer> create(const QString& file_path);

    // Creates a packet for transferring.
    std::unique_ptr<proto::file_transfer::Packet> readNextPacket();

private:
    FilePacketizer(QPointer<QFile>& file);

    QPointer<QFile> file_;

    qint64 file_size_ = 0;
    qint64 left_size_ = 0;

    Q_DISABLE_COPY(FilePacketizer)
};

} // namespace aspia

#endif // _ASPIA_HOST__FILE_PACKETIZER_H
