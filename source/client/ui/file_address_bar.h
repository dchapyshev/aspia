//
// PROJECT:         Aspia
// FILE:            client/ui/file_address_bar.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ADDRESS_BAR_H
#define _ASPIA_CLIENT__UI__FILE_ADDRESS_BAR_H

#include <QComboBox>

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileAddressBar : public QComboBox
{
    Q_OBJECT

public:
    FileAddressBar(QWidget* parent);
    virtual ~FileAddressBar();

    QString currentPath() const;
    QString itemPath(int index) const;
    void setCurrentPath(const QString& path);

signals:
    void addressChanged(const QString& address);

public slots:
    void updateDrives(const proto::file_transfer::DriveList& drive_list);
    void toChildFolder(const QString& child_name);
    void toParentFolder();
    void refresh();

private slots:
    void itemChanged(int index);
    void textChanged(const QString& text);

private:
    DISALLOW_COPY_AND_ASSIGN(FileAddressBar);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ADDRESS_BAR_H
