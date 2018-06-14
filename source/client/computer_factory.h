//
// PROJECT:         Aspia
// FILE:            client/computer_factory.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__COMPUTER_FACTORY_H
#define _ASPIA_CLIENT__COMPUTER_FACTORY_H

#include "protocol/address_book.pb.h"

namespace aspia {

class ComputerFactory
{
public:
    static proto::address_book::Computer defaultComputer();

    static proto::desktop::Config defaultDesktopManageConfig();
    static proto::desktop::Config defaultDesktopViewConfig();

    static void setDefaultDesktopManageConfig(proto::desktop::Config* config);
    static void setDefaultDesktopViewConfig(proto::desktop::Config* config);

private:
    Q_DISABLE_COPY(ComputerFactory)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__COMPUTER_FACTORY_H
