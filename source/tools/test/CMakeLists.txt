#
# Aspia Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

add_executable(aspia_test main.cc)
target_link_libraries(aspia_test
    aspia_base
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    Dbghelp)
set_target_properties(aspia_test PROPERTIES WIN32_EXECUTABLE TRUE)
qt5_import_plugins(aspia_test
    INCLUDE ""
    EXCLUDE ""
    INCLUDE_BY_TYPE ""
    EXCLUDE_BY_TYPE imageformats)
