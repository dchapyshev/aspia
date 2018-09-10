#
# Aspia Project
# Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_IPC
    ipc_channel.cc
    ipc_channel.h
    ipc_server.cc
    ipc_server.h)

file(GLOB_RECURSE IPC_HEADERS ${PROJECT_SOURCE_DIR}/ipc/*.h)
qt5_wrap_cpp(SOURCE_IPC_MOC ${IPC_HEADERS} OPTIONS "--no-notes")

source_group("" FILES ${SOURCE_IPC})
source_group(moc FILES ${SOURCE_IPC_MOC})

add_library(aspia_ipc STATIC ${SOURCE_IPC} ${SOURCE_IPC_MOC})
target_link_libraries(aspia_ipc
    aspia_base
    ${THIRD_PARTY_LIBS})
