#
# Aspia Project
# Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

list(APPEND SOURCE_CREDENTIALS
    credentials.def
    credential.cc
    credential.h
    dll_main.cc
    dll_main.h
    ipc_client.cc
    ipc_client.h
    logging.cc
    logging.h
    provider.cc
    provider.h
    resources.rc
    utils.h)

source_group("" FILES ${SOURCE_CREDENTIALS})

add_library(aspia_credentials SHARED ${SOURCE_CREDENTIALS})

target_link_libraries(aspia_credentials PRIVATE asio::asio advapi32 credui secur32 shlwapi)
