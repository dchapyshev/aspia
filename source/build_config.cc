//
// PROJECT:         Aspia
// FILE:            build_config.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "build_config.h"

#include <QtCore/QtPlugin>
Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin);

namespace aspia {

const unsigned short kDefaultHostTcpPort = 8050;

} // namespace aspia
