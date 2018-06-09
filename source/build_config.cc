//
// PROJECT:         Aspia
// FILE:            build_config.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "build_config.h"

#include <QtCore/QtPlugin>
Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN (QWindowsVistaStylePlugin);

namespace aspia {

const int kDefaultHostTcpPort = 8050;

} // namespace aspia
