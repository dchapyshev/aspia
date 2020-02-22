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

list(APPEND SOURCE_QT_BASE
    application.cc
    application.h
    locale_loader.cc
    locale_loader.h
    qt_logging.cc
    qt_logging.h
    qt_task_runner.cc
    qt_task_runner.h
    qt_xml_settings.cc
    qt_xml_settings.h)

source_group("" FILES ${SOURCE_QT_BASE})

add_library(aspia_qt_base STATIC ${SOURCE_QT_BASE})
target_link_libraries(aspia_qt_base
    aspia_base
    ${QT_LIBS}
    ${THIRD_PARTY_LIBS})
