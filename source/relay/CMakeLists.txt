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

include(tbb)

list(APPEND SOURCE_RELAY
    controller.cc
    controller.h
    main.cc
    pending_session.cc
    pending_session.h
    session.cc
    session.h
    session_key.cc
    session_key.h
    session_manager.cc
    session_manager.h
    settings.cc
    settings.h
    shared_pool.cc
    shared_pool.h)

list(APPEND SOURCE_RELAY_WIN
    win/service.cc
    win/service.h
    win/service_constants.cc
    win/service_constants.h)

source_group("" FILES ${SOURCE_RELAY})
source_group(win FILES ${SOURCE_RELAY_WIN})

add_executable(aspia_relay ${SOURCE_RELAY} ${SOURCE_RELAY_WIN} win/relay.rc)
set_target_properties(aspia_relay PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_relay
    aspia_base
    aspia_proto
    comsupp
    crypt32
    netapi32
    version
    ${THIRD_PARTY_LIBS})

if (USE_TBB)
    add_tbb(aspia_relay ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()
