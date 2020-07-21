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

add_library(aspia_desktop STATIC ${SOURCE_DESKTOP} ${SOURCE_DESKTOP_WIN})
target_link_libraries(aspia_desktop aspia_base ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_desktop_tests
        ${SOURCE_DESKTOP_UNIT_TESTS}
        ${SOURCE_DESKTOP_WIN_UNIT_TESTS})
    target_link_libraries(aspia_desktop_tests
        aspia_base
        aspia_desktop
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_desktop_tests COMMAND aspia_desktop_tests)
endif()

