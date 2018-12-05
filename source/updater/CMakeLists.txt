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

add_subdirectory(translations)

list(APPEND SOURCE_UPDATER
    update.h
    update_checker.cc
    update_checker.h
    update_dialog.cc
    update_dialog.h
    update_dialog.ui
    updater.cc
    updater.h)

list(APPEND SOURCE_UPDATER_RESOURCES
    resources/updater.qrc)

source_group("" FILES ${SOURCE_UPDATER})
source_group(resources FILES ${SOURCE_UPDATER_RESOURCES})
source_group(ui FILES ${SOURCE_UPDATER_UI})

add_library(aspia_updater STATIC
    ${SOURCE_UPDATER}
    ${SOURCE_UPDATER_RESOURCES}
    ${SOURCE_UPDATER_UI})
target_link_libraries(aspia_updater
    aspia_base
    aspia_network
    ${THIRD_PARTY_LIBS})
