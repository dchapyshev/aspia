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

collect_sources(SOURCE_BASE_FILES
    base_paths.cc
    base_paths.h
    file_enumerator.cc
    file_enumerator.h
    file_util.cc
    file_util.h)

if (APPLE)
    collect_sources(SOURCE_BASE_FILES
        base_paths_mac.mm)
endif()

collect_sources(SOURCE_BASE_FILES_TESTS
    base_paths_unittest.cc)
