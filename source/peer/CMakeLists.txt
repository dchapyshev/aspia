#
# Aspia Project
# Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_PEER
    client_authenticator.cc
    client_authenticator.h
    peer_controller.cc
    peer_controller.h
    peer_id.cc
    peer_id.h
    server_authenticator.cc
    server_authenticator.h
    server_authenticator_manager.cc
    server_authenticator_manager.h
    user.cc
    user.h)

source_group("" FILES ${SOURCE_PEER})

add_library(aspia_peer STATIC ${SOURCE_PEER})
target_link_libraries(aspia_peer aspia_base)
