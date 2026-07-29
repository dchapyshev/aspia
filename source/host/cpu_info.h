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

#ifndef HOST_CPU_INFO_H
#define HOST_CPU_INFO_H

namespace proto::system_info {
class Processor;
} // namespace proto::system_info

// Reports what the processor is: how it identifies itself, its caches and every feature it has.
// The way to ask depends on the architecture and on the system, and the answer does not: the
// features are resolved here and travel to the client as named values it displays without knowing
// either.
void fillProcessorInfo(proto::system_info::Processor* processor);

#endif // HOST_CPU_INFO_H
