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

#ifndef HOST_AGENT_MAIN_H
#define HOST_AGENT_MAIN_H

// Runs the host as a headless agent; |agent_type| is the "--agent" value straight from the command
// line ("desktop", "file" or "terminal"). The agent is the same aspia_host binary as the GUI, so it
// presents the same code identity to the OS - on macOS that is what lets it inherit the app's privacy
// (TCC) grants instead of being a separate app. Returns the process exit code.
int runAgent(int& argc, char* argv[], const char* agent_type);

#endif // HOST_AGENT_MAIN_H
