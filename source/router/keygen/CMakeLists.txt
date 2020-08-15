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
include(translations)

list(APPEND SOURCE_ROUTER_KEYGEN
    main.cc
    main_window.cc
    main_window.h
    main_window.ui
    keygen.rc)

list(APPEND SOURCE_ROUTER_KEYGEN_RESOURCES resources/keygen.qrc)

source_group("" FILES ${SOURCE_ROUTER_KEYGEN})
source_group(moc FILES ${SOURCE_ROUTER_KEYGEN_MOC})
source_group(resources FILES ${SOURCE_ROUTER_KEYGEN_RESOURCES})

add_executable(aspia_router_keygen
    ${SOURCE_ROUTER_KEYGEN}
    ${SOURCE_ROUTER_KEYGEN_RESOURCES})
set_target_properties(aspia_router_keygen PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_router_keygen PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_router_keygen
    aspia_base
    aspia_crypto
    aspia_net
    aspia_qt_base
    ${QT_LIBS}
    ${PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})

if (USE_TBB)
    add_tbb(aspia_router_keygen ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()
