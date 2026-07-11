//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef HOST_HOST_CONSTANTS_H
#define HOST_HOST_CONSTANTS_H

// Well-known IPC channel the desktop agent and the service rendezvous on. Fixed (not a per-attach
// random name) because on macOS the desktop agent is launched by launchd - the service is not its
// parent and cannot hand it a unique name - so both ends must agree on a constant, compiled into
// both. Used uniformly on all platforms; a stale agent from a previous session cannot cause trouble
// because the newest connection wins (see DesktopManager::onIpcNewConnection). The connecting peer
// is still authenticated by executable path.
extern const char kDesktopAgentChannelId[];

// Well-known IPC channel the host UI (aspia_host --hidden) and the service's UserSession rendezvous
// on. Fixed for the same reason as kDesktopAgentChannelId: both ends are the same aspia_host binary,
// so the name is compiled into both rather than generated and handed over. The connecting peer is
// authenticated by executable path and session id in UserSession::onIpcNewConnection().
extern const char kHostUiChannelId[];

#endif // HOST_HOST_CONSTANTS_H
