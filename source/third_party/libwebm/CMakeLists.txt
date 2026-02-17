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

list(APPEND SOURCE_LIBWEBM
    common/webmids.h
    mkvmuxer/mkvmuxer.cc
    mkvmuxer/mkvmuxer.h
    mkvmuxer/mkvmuxertypes.h
    mkvmuxer/mkvmuxerutil.cc
    mkvmuxer/mkvmuxerutil.h
    mkvmuxer/mkvwriter.cc
    mkvmuxer/mkvwriter.h
    mkvparser/mkvparser.cc
    mkvparser/mkvparser.h
    mkvparser/mkvreader.cc
    mkvparser/mkvreader.h)

source_group("" FILES ${SOURCE_LIBWEBM})

add_library(libwebm STATIC ${SOURCE_LIBWEBM})
target_link_libraries(libwebm)
target_include_directories(libwebm PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(libwebm PROPERTIES FOLDER third_party)

if (WIN32)
    target_compile_definitions(libwebm PRIVATE -D_CRT_SECURE_NO_WARNINGS)
endif()
