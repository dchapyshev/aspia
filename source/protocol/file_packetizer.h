//
// PROJECT:         Aspia
// FILE:            protocol/file_packetizer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILE_PACKETIZER_H
#define _ASPIA_PROTOCOL__FILE_PACKETIZER_H

#include <QFile>
#include <QPointer>
#include <QString>
#include <memory>

#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FilePacketizer
{
public:
    ~FilePacketizer() = default;

    // Creates an instance of the class.
    // Parameter |file_path| contains the full path to the file.
    // If the specified file can not be opened for reading, then returns nullptr.
    static std::unique_ptr<FilePacketizer> Create(const QString& file_path);

    // Creates a packet for transferring.
    std::unique_ptr<proto::file_transfer::Packet> CreateNextPacket();

    qint64 LeftSize() const { return left_size_; }

private:
    FilePacketizer(QPointer<QFile>& file);

    QPointer<QFile> file_;

    qint64 file_size_ = 0;
    qint64 left_size_ = 0;

    Q_DISABLE_COPY(FilePacketizer)
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILE_PACKETIZER_H
