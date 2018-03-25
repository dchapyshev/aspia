//
// PROJECT:         Aspia
// FILE:            protocol/file_depacketizer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H
#define _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H

#include <QFile>
#include <QPointer>
#include <QString>
#include <memory>

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileDepacketizer
{
public:
    ~FileDepacketizer() = default;

    static std::unique_ptr<FileDepacketizer> Create(const QString& file_path, bool overwrite);

    // Reads the packet and writes its contents to a file.
    bool ReadNextPacket(const proto::file_transfer::Packet& packet);

    qint64 LeftSize() const { return left_size_; }

private:
    FileDepacketizer(QPointer<QFile>& file_stream);

    QPointer<QFile> file_;

    qint64 file_size_ = 0;
    qint64 left_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileDepacketizer);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__FILE_DEPACKETIZER_H
